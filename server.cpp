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
mutex vector_mutex;
mutex needMore_mutex;
bool needMore = false;
bool endTransmission = false;
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
    while (!unreceived_set.empty()) {
//        cout << "waiting for signal" << endl;
        while (needMore) {
            if (unreceived_set.empty()) {
                return;
            }
            cout << "Total remaining: " << unreceived_set.size() << endl;
            int tmp = 0;
            auto iter = unreceived_set.begin();
            while (iter != unreceived_set.end()) {
                int ack_num[340];
                char ack_num_buf[sizeof(ack_num)];
                for (int i = 0; i < sizeof(ack_num) / sizeof(int); i++) {
                    if (iter == unreceived_set.end()) {
                        ack_num[i] = -1;
                    } else {
                        ack_num[i] = *iter;
                        iter++;
                        tmp++;
                    }
                }
                memcpy(ack_num_buf, (char *) &ack_num, sizeof(ack_num_buf));
                for (int j = 0; j < 1; j++) {
                    int sent = sendto(ack_sfd, ack_num_buf, sizeof(ack_num_buf), 0, (sockaddr *) &clientACKInfo,
                                      clientInfoLen);
                }
            }
        }
        needMore_mutex.lock();
        needMore = false;
        needMore_mutex.unlock();
    }
}

void sendEND() {
    int next = -1;
    for (int j = 0; j < 5; j++) {
        int sent = sendto(ack_sfd, (char *) &next, sizeof(next), 0, (sockaddr *) &clientACKInfo,
                          clientInfoLen);
    }
}

void receive() {
    char recv_buffer[sizeof(DTO<packSize>)];
    DTO<packSize> dto;
    while (!unreceived_set.empty()) {
        bytes = recvfrom(rcv_sfd, recv_buffer, sizeof(recv_buffer), 0, (struct sockaddr *) &clientInfo,
                         &clientInfoLen);
//        auto *dto = new DTO<packSize>;
        memcpy(&dto, recv_buffer, sizeof(DTO<packSize>));
        memset(recv_buffer, 0, sizeof(recv_buffer));
        if (dto.offset == -1) {
            needMore_mutex.lock();
            if (!needMore) {
                needMore = true;
            }
            needMore_mutex.unlock();
            continue;
        }
        if (unreceived_set.count(dto.offset) == 0) {
            continue;
        }
        string md5 = get_str_md5(dto.data, packSize);
//        cout << "md5: " << md5;
        if (strcmp(md5.c_str(), dto.md5) != 0) {
            std::cout << "wrong md5" << std::endl;
            continue;
        }
        ofs.seekp(dto.offset * packSize, ios::beg);
        if (dto.offset != packNum - 1) {
            ofs.write(dto.data, packSize);
        } else {
            ofs.write(dto.data, fileSize - packSize * (packNum - 1));
        }
        unreceived_set.erase(dto.offset);
//        cout << " ack: " << dto->offset << "size: " << set.size() << endl;
    }
//        delete dto;
}


int main(int argc, char *argv[]) {
    ofs.open(result_file, ios::binary);
    rcv_sfd = buildUDPSocket(SERVER_PORT);
    ack_sfd = buildUDPSocket(SERVER_ACK_PORT);

    printf("Server UDP socket on Port: %d\n", SERVER_PORT);


    initial();

    for (int i = 0; i < packNum; ++i) {
        unreceived_set.insert(i);
    }

    std::cout << "Transmission start: " << std::endl;
    timeval beginTime;
    gettimeofday(&beginTime, NULL);

    thread sendACK_thread(sendACK);
    thread rcv_thread(receive);

    sendACK_thread.join();
    rcv_thread.join();

    sendEND();

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
