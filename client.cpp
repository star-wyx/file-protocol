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
int fd;
char *file_buff;
const int packSize = 5000;
int times = 3;

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

    int size;
    struct stat s;

    /* Get the size of the file. */
    int status = fstat(fd, &s);
    size = s.st_size;

    file_buff = (char *) mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);

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
//    timeval begin;
//    gettimeofday(&begin, NULL);
//    ifs.clear();0
//    ifs.seekg(offset * packSize, ios::beg);
//    ifs.read(buffer, packSize);
    memcpy(buffer, &file_buff[offset * packSize], packSize);
//    timeval end;
//    gettimeofday(&end, NULL);
//    cout << "read delay: " << (end.tv_sec - begin.tv_sec) * 1000000 + (end.tv_usec - begin.tv_usec) << endl;
    return buffer;
}

void updateSet() {
    int add[340];
    char add_buffer[sizeof(add)];
    while (!endTransmission) {
        int bytes = recvfrom(ack_sfd, add_buffer, sizeof(add_buffer), 0, (struct sockaddr *) &serverACKInfo,
                             &serverInfoLen);
        memcpy(&add, add_buffer, sizeof(add_buffer));
        if (add[0] == -1) {
            cout << "endTransmission True" << endl;
            endTransmission = true;
        } else {
            for (int i = 0; i < sizeof(add) / sizeof(int); i++) {
                if (add[i] == -1) {
                    break;
                }
                if (send_deque.empty() || std::find(send_deque.begin(), send_deque.end(), add[i]) == send_deque.end()) {
                    send_deque.push_back(add[i]);
                }
            }
        }
    }

}

void send_data() {
    while (!endTransmission) {
        if (!send_deque.empty()) {
            cout << "remain: " << send_deque.size() << endl;
        }
        int deque_size = send_deque.size();
        int tmp = 0;
        while (!send_deque.empty()) {
            if (endTransmission) {
                break;
            }
            vector_mutex.lock();
            int i = send_deque.front();
            send_deque.pop_front();
            vector_mutex.unlock();
            tmp += 1;
            for (int j = 0; j < times; j++) {
                char *buffer = read(i);
                string md5 = get_str_md5(buffer, packSize);
                auto *dto = new DTO<packSize>(md5, i, buffer, packSize);
                int sent = sendto(send_sfd, (char *) dto, sizeof(DTO<packSize>), 0, (sockaddr *) &serverInfo,
                                  sizeof(serverInfo));

                cout << " " << dto->offset << endl;

                delete dto;
                free(buffer);
            }
            usleep(50);
        }
        auto *dto = new DTO<packSize>();
        memset(dto, 0, sizeof(DTO<packSize>));
        dto->offset = -1;
        int sent = sendto(send_sfd, (char *) dto, sizeof(DTO<packSize>), 0, (sockaddr *) &serverInfo,
                          sizeof(serverInfo));
        delete dto;
    }
}

int main(int argc, char *argv[]) {
    const char *SERVER_IP = argv[1];
    char *file = argv[2];
    fd = open(argv[2], O_RDONLY);

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
