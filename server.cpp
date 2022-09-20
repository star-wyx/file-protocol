// Created by Lenovo on 9/16/2022.
//

#include "basic.h"


int buildTCPSocket() {
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) {
        std::cout << "socket failed" << std::endl;
        exit(1);
    }
    struct sockaddr_in local;
    memset(&local, 0, sizeof local);
    local.sin_family = AF_INET;
    local.sin_port = htons(SERVER_TCP_PORT);
    local.sin_addr.s_addr = INADDR_ANY;
    if (bind(sfd, (struct sockaddr *) &local, sizeof(struct sockaddr)) < 0) {
        std::cout << "bind failed" << std::endl;
        exit(2);
    }

    return sfd;
}

int TCPsocketAccept(int sfd, struct sockaddr_in *cli_addr) {
    int newsfd;
    socklen_t cli_len = sizeof(*cli_addr);
    memset(cli_addr, 0, sizeof(*cli_addr));
    int lis = listen(sfd, 10);
    if (lis == -1) {
        std::cout << "listen failed: " << lis << std::endl;
        exit(3);
    }

    newsfd = accept(sfd, (struct sockaddr *) cli_addr, &cli_len);

    if (newsfd < 0) {
        std::cout << "Accept TCP connection failed: " << newsfd << std::endl;
        exit(4);
    }

    return newsfd;
}

int buildUDPSocket(uint16_t port) {
    int sfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sfd < 0) {
        std::cout << "socket failed" << std::endl;
        exit(1);
    }
    timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 250000;
    struct sockaddr_in local;
    memset(&local, 0, sizeof local);
    setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, sizeof(timeout));
    local.sin_family = AF_INET;
    local.sin_port = htons(port);
    local.sin_addr.s_addr = INADDR_ANY;
    if (bind(sfd, (struct sockaddr *) &local, sizeof(struct sockaddr)) < 0) {
        std::cout << "bind failed" << std::endl;
        exit(2);
    }
    return sfd;
}

void sendACK(unordered_set<int> &set, int packNum, int tcp_newsfd) {
    cout << "Total remaining: " << set.size() << endl;
    vector<int> v;
    // for (int i = 0; i < packNum; i++) {
    //     if (set.count(i) > 0) {
    //         continue;
    //     }
    //     v.push_back(i);
    //     // cout << i << " ";
    // }
    int non_acked[100];
    auto iter = set.begin();
    for (int i = 0; i < sizeof(non_acked) / sizeof(int); i++) {
        if (iter == set.end()) {
            non_acked[i] = -1;
        } else {
            non_acked[i] = *iter;
//            cout << non_acked[i] << " ";
            iter++;
        }
    }
//    cout << endl;
    // int batch = 2000;
    // for(int b = 0; b<20000/batch; b++){
    //     char buff[batch];
    //     for(int t = 0; t<batch; t++){
    //         buff[t] = non_acked[batch*b+t];
    //     }
    //     send(tcp_newsfd, &buff, sizeof(buff), 0);
         
    // }
    int tmp = send(tcp_newsfd, (char *) &non_acked, sizeof(non_acked), 0);
}


int main(int argc, char *argv[]) {
    int bytes;
    struct sockaddr_in clientInfo;
    socklen_t clientInfoLen = sizeof(clientInfo);
    unordered_set<int> set;
    int largest_no;
    int sent;
    const int packSize = 5000;
    int packNum = 0;
    int fileSize = 1073741824;
    string result_file = "result.bin";
    ofstream ofs(result_file, ios::binary);

    int socket = buildUDPSocket(SERVER_PORT);
    int tcp_sfd = buildTCPSocket();

    printf("Server UDP socket on Port: %d\n", SERVER_PORT);
    printf("Server TCP socket on Port: %d\n", SERVER_TCP_PORT);

    struct sockaddr_in cli_tcp_addr;
    int tcp_newsfd = TCPsocketAccept(tcp_sfd, &cli_tcp_addr);

    printf("Client Port: %d has connected \n", SERVER_TCP_PORT);

    char recv_buffer[sizeof(DTO<packSize>)];

    // for (int i = 0; i < 3; i++) {
    //     bytes = recvfrom(socket, recv_buffer, sizeof(recv_buffer), 0, (struct sockaddr *) &clientInfo,
    //                      &clientInfoLen);
    //     auto *dto = new DTO<packSize>;
    //     memcpy(dto, recv_buffer, sizeof(DTO<packSize>));
    //     if (dto->offset == -1) {
    //         packNum = dto->stream;
    //         delete dto;
    //         break;
    //     }
    // }

    bytes = read(tcp_newsfd, recv_buffer, sizeof(recv_buffer));
    auto *dto = new DTO<packSize>;
    memcpy(dto, recv_buffer, sizeof(DTO<packSize>));
    if (dto->offset == -1) {
        packNum = dto->stream;
        delete dto;
    }

    if (packNum == 0) {
        exit(1);
    }
    std::cout << "Header received" << std::endl;
    std::cout << "Packet num: " << packNum << std::endl;

    for(int i = 0; i<packNum; ++i){
        set.insert(i);
    }

    std::cout << "Transmission start: " << std::endl;
    timeval beginTime;
    gettimeofday(&beginTime, NULL);

    while (!set.empty()) {
        bytes = recvfrom(socket, recv_buffer, sizeof(recv_buffer), 0, (struct sockaddr *) &clientInfo,
                         &clientInfoLen);
        if (bytes < 0) {
            timeval beginTime_temp;
            gettimeofday(&beginTime_temp, NULL);
            sendACK(set, packNum, tcp_newsfd);
            timeval endTime_temp;
            gettimeofday(&endTime_temp, NULL);
            cout << "SendACK Duration: " << endTime_temp.tv_sec - beginTime_temp.tv_sec << "s " << endTime_temp.tv_usec - beginTime_temp.tv_usec << "us"
         << endl;
            continue;
        }
        auto *dto = new DTO<packSize>;
        memcpy(dto, recv_buffer, sizeof(DTO<packSize>));
        if (dto->offset == -1) {
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
        set.erase(dto->offset);
//        cout << " ack: " << dto->offset << "size: " << set.size() << endl;
        delete dto;
    }

    sendACK(set, packNum, tcp_newsfd);

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

    close(socket);
    close(tcp_newsfd);
    close(tcp_sfd);

    return 0;
}
