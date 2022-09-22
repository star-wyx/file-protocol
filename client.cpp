//
// Created by Lenovo on 9/16/2022.
//
#include "basic.h"


using namespace std;


int packNum;
int fileSize;
ifstream ifs;
std::deque<int> send_deque;
int send_sfd;
int ack_sfd;
sockaddr_in serverInfo;
sockaddr_in serverACKInfo;
socklen_t serverInfoLen = sizeof(serverInfo);
bool endTransmission = false;
mutex vector_mutex;
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
        send_deque.push_back(i);
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
    int add;
    char add_buffer[sizeof(add)];
//    timeval timeout;
//    timeout.tv_sec = 0;
//    timeout.tv_usec = 250000;
//    setsockopt(ack_sfd, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, sizeof(timeout));
    while (!endTransmission) {
        int bytes = recvfrom(ack_sfd, add_buffer, sizeof(add_buffer), 0, (struct sockaddr *) &serverACKInfo,
                             &serverInfoLen);
        memcpy(&add, add_buffer, sizeof(add_buffer));
        if (add == -1) {
            cout << "endTransmission True" << endl;
            endTransmission = true;
        } else {
            vector_mutex.lock();
            if (send_deque.empty() || (!send_deque.empty() && add != send_deque.back())) {
                send_deque.push_back(add);
            }
            vector_mutex.unlock();
        }
    }

}

void send_data() {
    while (!endTransmission) {
        cout << "remain: " << send_deque.size() << endl;
        while (!send_deque.empty()) {
            vector_mutex.lock();
            int i = send_deque.front();
            send_deque.pop_front();
            vector_mutex.unlock();
            for (int j = 0; j < times; j++) {
                char *buffer = read(i);
                string md5 = get_str_md5(buffer, packSize);
                auto *dto = new DTO<packSize>(md5, i, buffer, packSize);
                int sent = sendto(send_sfd, (char *) dto, sizeof(DTO<packSize>), 0, (sockaddr *) &serverInfo,
                                  sizeof(serverInfo));
                delete dto;
            }
        }
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

    thread ack_thread(updateSet);
    thread send_thread(send_data);

    send_thread.join();
    ack_thread.join();

    ifs.close();
    string md5;
    get_file_md5(file, md5);
    cout << md5 << endl;

    close(send_sfd);
    close(ack_sfd);

    return 0;
}
