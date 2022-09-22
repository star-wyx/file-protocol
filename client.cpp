//
// Created by Lenovo on 9/16/2022.
//
#include "basic.h"


using namespace std;


int packNum;
int fileSize;
ifstream ifs;
vector<int> unreceived_set;
int send_sfd;
int ack_sfd;
sockaddr_in serverInfo;
sockaddr_in serverACKInfo;
socklen_t serverInfoLen = sizeof(serverInfo);
const int packSize = 5000;
int times = 1;

int buildUDPSocket(uint16_t port) {
    char ip[] = "127.0.0.1";
    int sfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sfd < 0) {
        std::cout << "socket failed" << std::endl;
        exit(1);
    }
    struct sockaddr_in local;
    memset(&local, 0, sizeof local);
    local.sin_family = AF_INET;
    local.sin_port = htons(port);
    local.sin_addr.s_addr = inet_addr(ip);
    return sfd;
}

void initial() {
    if (!ifs.good()) {
        cout << "fail to open file" << endl;
        exit(1);
    }
    ifs.seekg(0, ios::end);
    fileSize = ifs.tellg();
    if ((fileSize % packSize) != 0) {
        packNum = fileSize / packSize + 1;
    } else {
        packNum = fileSize / packSize;
    }
    ifs.seekg(0, ios::beg);
    cout << "packNum: " << packNum << endl;

    char fileSizebuffer[sizeof(fileSize)];
    memcpy(fileSizebuffer, (const void *) &fileSize, sizeof(fileSize));
    int sent = sendto(ack_sfd, fileSizebuffer, sizeof(fileSizebuffer), 0, (sockaddr *) &serverACKInfo,
                      sizeof(serverInfo));

    if (sent < 0)
        printf("ERROR writing to socket");

    for (int i = 0; i < packNum; i++) {
        unreceived_set.push_back(i);
    }
}

char *read(int offset) {
    char *buffer = (char *) malloc(packSize * sizeof(char));
    ifs.clear();
    ifs.seekg(offset * packSize, ios::beg);
    ifs.read(buffer, packSize);
    return buffer;
}

void updateSet() {
    int non_acked[100];
    unreceived_set = {};
    char non_acked_buffer[sizeof(non_acked)];
    timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 250000;
    setsockopt(ack_sfd, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, sizeof(timeout));
    while (true) {
        int bytes = recvfrom(ack_sfd, non_acked_buffer, sizeof(non_acked_buffer), 0, (struct sockaddr *) &serverACKInfo,
                             &serverInfoLen);
        if (bytes < 0) {
            break;
        }
    }
    memcpy(non_acked, non_acked_buffer, sizeof(non_acked_buffer));

    for (int i = 0; i < sizeof(non_acked) / sizeof(int); i++) {
        if (non_acked[i] == -1) {
            break;
        }
        unreceived_set.push_back(non_acked[i]);
    }
}

int main(int argc, char *argv[]) {
    const char *SERVER_IP = argv[1];
    char *file = argv[2];

    ifs.open(file, ios::binary | ios::in);
    send_sfd = buildUDPSocket(CLIENT_PORT);
    ack_sfd = buildUDPSocket(CLIENT_ACK_PORT);
    serverInfo = buildSockaddr_in(SERVER_IP, SERVER_PORT);
    serverACKInfo = buildSockaddr_in(SERVER_IP, SERVER_ACK_PORT);
    serverInfoLen = sizeof(serverInfo);

    initial();

    while (!unreceived_set.empty()) {
        cout << "remain: " << unreceived_set.size() << endl;
        for (int i: unreceived_set) {
            for (int j = 0; j < times; j++) {
                char *buffer = read(i);
                string md5 = get_str_md5(buffer, packSize);
                auto *dto = new DTO<packSize>(md5, i, buffer, packSize);
                int sent = sendto(send_sfd, (char *) dto, sizeof(DTO<packSize>), 0, (sockaddr *) &serverInfo,
                                  sizeof(serverInfo));
                delete dto;
            }
        }
        updateSet();
    }


    ifs.close();
    string md5;
    get_file_md5(file, md5);
    cout << md5 << endl;

    close(send_sfd);
    close(ack_sfd);

    return 0;
}
