//
// Created by Lenovo on 9/16/2022.
//
#include "basic.h"


using namespace std;


int buildTCPSocket(const char ip[], uint16_t client_port) {
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) {
        std::cout << "socket failed" << std::endl;
        return 1;
    }
    struct sockaddr_in connect_addr;
    // struct hostent *host_addr = gethostbyname(ip);
    // if(host_addr == NULL){
    //     std::cout<<"No such host ip found: "<<ip<<endl;
    //     exit(3);
    // }
    memset(&connect_addr, 0, sizeof connect_addr);
    connect_addr.sin_family = AF_INET;
    connect_addr.sin_port = htons(SERVER_TCP_PORT);
    //inet_pton(AF_INET, ip, &connect_addr.sin_addr);
    connect_addr.sin_addr.s_addr = inet_addr(ip);
    int con = connect(sfd, (struct sockaddr *) &connect_addr, sizeof(sockaddr));
    if (con < 0) {
        std::cout << "errno is: " << errno << " error information: " << strerror(errno) << std::endl;
        std::cout << "connect failed" << std::endl;
        exit(2);
    }
    return sfd;
}

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


sockaddr_in buildSockaddr_in(const char *ip, int port) {
    struct sockaddr_in res;
    memset(&res, 0, sizeof(res));
    res.sin_family = AF_INET;
    res.sin_addr.s_addr = inet_addr(ip);
    res.sin_port = htons(port);
    return res;
}

char *read(int offset, int packSize, ifstream &ifs) {
    char *buffer = (char *) malloc(packSize * sizeof(char));
    ifs.clear();
    ifs.seekg(offset * packSize, ios::beg);
    ifs.read(buffer, packSize);
    return buffer;
}

void updateSet(int tcp_sfd, vector<int> &set) {
    int non_acked[100];
    set = {};
    int tmp = recv(tcp_sfd, &non_acked, sizeof(non_acked), 0);

    for (int i = 0; i < sizeof(non_acked) / sizeof(int); i++) {
        if (non_acked[i] == -1) {
            break;
        }
//        cout << non_acked[i] << " ";
        set.push_back(non_acked[i]);
    }
}

int main(int argc, char *argv[]) {
    const char *SERVER_IP = argv[1];
    char *file = argv[2];
    int packNum;
    long fileSize = 0;
    const int packSize = 5000;
    int times = 5;
    vector<int> set;

    sockaddr_in serverInfo = buildSockaddr_in(SERVER_IP, SERVER_PORT);
    socklen_t serverInfoLen = sizeof(serverInfo);

    ifstream ifs(file, ios::binary | ios::in);
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

    int socket = buildUDPSocket(CLIENT_PORT);
    int tcp_sfd = buildTCPSocket(SERVER_IP, SERVER_TCP_PORT);

    // for (int j = 0; j < times; j++) {
    //     DTO<packSize> *dto = new DTO<packSize>(packNum, -1);
    //     int sent = sendto(socket, (char *) dto, sizeof(DTO<packSize>), 0, (sockaddr *) &serverInfo, sizeof(serverInfo));
    //     delete dto;
    // }

    DTO<packSize> *dto = new DTO<packSize>(packNum, -1);
    int sent = write(tcp_sfd, (char *) dto, sizeof(DTO<packSize>));
    delete dto;

    if (sent < 0)
        printf("ERROR writing to socket");

    for (int i = 0; i < packNum; i++) {
        set.push_back(i);
    }

    while (set.size() != 0) {
        cout << "remain: " << set.size() << endl;
        for (auto iter = set.begin(); iter != set.end(); iter++) {
            int i = *iter;
            // if (set.size() != packNum) {
            //     cout << i << ": ";
            // }
            for (int j = 0; j < times; j++) {
                char *buffer = read(i, packSize, ifs);
                string md5 = get_str_md5(buffer, packSize);
//            cout << "md5: " << md5 << endl;
                DTO<packSize> *dto = new DTO<packSize>(md5, 0, i, buffer, packSize);
                int sent = sendto(socket, (char *) dto, sizeof(DTO<packSize>), 0, (sockaddr *) &serverInfo,
                                  sizeof(serverInfo));
                // if (set.size() != packNum) {
                //     cout << sent<<", ";
                // }
                delete buffer;
                delete dto;
            }
            // cout <<endl;
        }
        updateSet(tcp_sfd, set);
    }


    ifs.close();
    string md5;
    get_file_md5(file, md5);
    cout << md5 << endl;

    close(socket);
    close(tcp_sfd);

    return 0;
}
