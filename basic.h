//
// Created by Lenovo on 9/16/2022.
//

#ifndef EE542_LINABELL_TEAM_BASIC_H
#define EE542_LINABELL_TEAM_BASIC_H

#include <sys/time.h>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <fstream>
#include <thread>
#include "stdlib.h"
#include<bits/stdc++.h>
#include <unistd.h>
#include "map"
#include "md5.h"
#include <netdb.h>

#define SERVER_PORT 60001
#define CLIENT_PORT 60002
#define SERVER_TCP_PORT 60003
#define CLIENT_TCP_PORT 60004

#define SERVER_ACK_PORT 60005
#define CLIENT_ACK_PORT 60006

using namespace std;

template<int data_length>
class DTO {
public:
    char md5[33];
    int offset;
    char data[data_length];

    DTO(string md5, int offset, char *data, int packSize) {
        strcpy(this->md5, md5.data());
        //char array the last element should be 0
        md5[32] = '0';
        this->offset = offset;
        memcpy(this->data, data, packSize);
    }

    DTO() {

    }
};

double difftimeval(const struct timeval *start, const struct timeval *end) {
    double d;
    time_t s;
    suseconds_t u;

    s = start->tv_sec - end->tv_sec;
    u = start->tv_usec - end->tv_usec;
    //if (u < 0)
    //        --s;

    d = s;
    d *= 1000000.0;//1 秒 = 10^6 微秒
    d += u;

    return d;
}

int difftime(struct timeval *end) {
    timeval start;
    gettimeofday(&start, NULL);

    return (((start.tv_sec - end->tv_sec) * 1000000) +
            (start.tv_usec - end->tv_usec)) / 1000;
}

sockaddr_in buildSockaddr_in(const char *ip, int port) {
    struct sockaddr_in res;
    memset(&res, 0, sizeof(res));
    res.sin_family = AF_INET;
    res.sin_addr.s_addr = inet_addr(ip);
    res.sin_port = htons(port);
    return res;
}

#endif //EE542_LINABELL_TEAM_BASIC_H
