// Created by Lenovo on 9/16/2022.
//

#include "basic.h"


int packNum;
int fileSize;
ofstream ofs;
unordered_set<int> unreceived_set;
int rcv_sfd;
int ack_sfd;
struct sockaddr_in clientInfo;
struct sockaddr_in clientACKInfo;
socklen_t clientInfoLen = sizeof(clientInfo);
int bytes;
int largest_no;
int sent;
const int packSize = 5000;
string result_file = "result.bin";


int buildUDPSocket(uint16_t port) {
    int sfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sfd < 0) {
        std::cout << "socket failed" << std::endl;
        exit(1);
    }
//    timeval timeout;
//    timeout.tv_sec = 0;
//    timeout.tv_usec = 250000;
    struct sockaddr_in local;
    memset(&local, 0, sizeof local);
//    setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, sizeof(timeout));
    local.sin_family = AF_INET;
    local.sin_port = htons(port);
    local.sin_addr.s_addr = INADDR_ANY;
    if (bind(sfd, (struct sockaddr *) &local, sizeof(struct sockaddr)) < 0) {
        std::cout << "bind failed" << std::endl;
        exit(2);
    }
    return sfd;
}

void initial() {
    char fileSizebuffer[sizeof(fileSize)];
    bytes = recvfrom(ack_sfd, fileSizebuffer, sizeof(fileSizebuffer), 0, (struct sockaddr *) &clientACKInfo,
                     &clientInfoLen);
    memcpy(&fileSize, fileSizebuffer, sizeof(fileSize));
    if ((fileSize % packSize) != 0) {
        packNum = fileSize / packSize + 1;
    } else {
        packNum = fileSize / packSize;
    }

    if (packNum == 0) {
        exit(1);
    }
    std::cout << "Header received" << std::endl;
    std::cout << "Packet num: " << packNum << std::endl;
    std::cout << "File size: " << fileSize << std::endl;
}

void sendACK() {
    cout << "Total remaining: " << unreceived_set.size() << endl;
    vector<int> v;
    int non_acked[100];
    auto iter = unreceived_set.begin();
    for (int i = 0; i < sizeof(non_acked) / sizeof(int); i++) {
        if (iter == unreceived_set.end()) {
            non_acked[i] = -1;
        } else {
            non_acked[i] = *iter;
//            cout << non_acked[i] << " ";
            iter++;
        }
    }
    for (int i = 0; i < 5; i++) {
        int sent = sendto(ack_sfd, (char *) &non_acked, sizeof(non_acked), 0, (sockaddr *) &clientACKInfo,
                          clientInfoLen);
    }
}


int main(int argc, char *argv[]) {
    ofs.open(result_file, ios::binary);
    rcv_sfd = buildUDPSocket(SERVER_PORT);
    ack_sfd = buildUDPSocket(SERVER_ACK_PORT);

    printf("Server UDP socket on Port: %d\n", SERVER_PORT);

    char recv_buffer[sizeof(DTO<packSize>)];

    initial();

    for (int i = 0; i < packNum; ++i) {
        unreceived_set.insert(i);
    }

    std::cout << "Transmission start: " << std::endl;
    timeval beginTime;
    gettimeofday(&beginTime, NULL);

    while (!unreceived_set.empty()) {
        timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 250000;
        setsockopt(rcv_sfd, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, sizeof(timeout));
        bytes = recvfrom(rcv_sfd, recv_buffer, sizeof(recv_buffer), 0, (struct sockaddr *) &clientInfo,
                         &clientInfoLen);
        if (bytes < 0) {
            timeval beginTime_temp;
            gettimeofday(&beginTime_temp, NULL);
            sendACK();
            timeval endTime_temp;
            gettimeofday(&endTime_temp, NULL);
            cout << "SendACK Duration: " << endTime_temp.tv_sec - beginTime_temp.tv_sec << "s "
                 << endTime_temp.tv_usec - beginTime_temp.tv_usec << "us"
                 << endl;
            continue;
        }
        auto *dto = new DTO<packSize>;
        memcpy(dto, recv_buffer, sizeof(DTO<packSize>));
        memset(recv_buffer, 0, sizeof(recv_buffer));
        if (dto->offset == -1 or unreceived_set.count(dto->offset) == 0) {
            continue;
        }
        string md5 = get_str_md5(dto->data, packSize);
//        cout << "md5: " << md5;
        if (strcmp(md5.c_str(), dto->md5) != 0) {
            std::cout << "wrong md5" << std::endl;
            continue;
        }
        ofs.seekp(dto->offset * packSize, ios::beg);
        if (dto->offset != packNum - 1) {
            ofs.write(dto->data, packSize);
        } else {
            ofs.write(dto->data, fileSize - packSize * (packNum - 1));
        }
        unreceived_set.erase(dto->offset);
//        cout << " ack: " << dto->offset << "size: " << set.size() << endl;
        delete dto;
    }

    sendACK();

    cout << "Transmission end. " << endl;
    timeval endTime;
    gettimeofday(&endTime, NULL);
    cout << "Duration: " << endTime.tv_sec - beginTime.tv_sec << "s " << endTime.tv_usec - beginTime.tv_usec << "us"
         << endl;
    cout << "Throughput: " << fileSize / (endTime.tv_sec - beginTime.tv_sec) * 8 << "bps" << endl;

    ofs.close();
    string md5;
    get_file_md5(result_file, md5);
    cout << md5 << endl;

    close(rcv_sfd);
    close(ack_sfd);

    return 0;
}
