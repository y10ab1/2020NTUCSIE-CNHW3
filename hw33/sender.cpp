#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <iostream>
#include <string>
#include <vector>
#include <queue>
#include <algorithm>
#include "opencv2/opencv.hpp"

#define datasize 4096

using namespace cv;
using namespace std;

typedef struct
{
    int length;
    int seqNumber;
    int ackNumber;
    int fin;
    int syn;
    int ack;
    int last;
} header;

typedef struct
{
    header head;
    char data[datasize];
} segment;

bool Segcmp(segment s1, segment s2)
{
    return s1.head.seqNumber < s2.head.seqNumber;
}

void setIP(char *dst, char *src)
{
    if (strcmp(src, "0.0.0.0") == 0 || strcmp(src, "local") == 0 || strcmp(src, "localhost"))
    {
        sscanf("127.0.0.1", "%s", dst);
    }
    else
    {
        sscanf(src, "%s", dst);
    }
}

int main(int argc, char *argv[])
{
    int sendersocket, portNum, nBytes;
    char videoname[1000];
    segment Packet;
    struct sockaddr_in sender, agent;
    socklen_t sender_size, agent_size;
    deque<segment> ResendPKT, SentPKT, TTTTTTTT;
    char ip[2][50];
    int port[2], i;

    int windowSize = 1, threshold = 16;

    if (argc != 6)
    {
        fprintf(stderr, "用法: %s <sender IP> <agent IP> <sender port> <agent port> <videoname>\n", argv[0]);
        fprintf(stderr, "例如: ./sender 127.0.0.1 127.0.0.1 8887 8888 ResendPKT.mpg\n");
        exit(1);
    }
    else
    {
        setIP(ip[0], argv[1]);
        setIP(ip[1], argv[2]);

        sscanf(argv[3], "%d", &port[0]);
        sscanf(argv[4], "%d", &port[1]);

        sscanf(argv[5], "%s", videoname);
    }

    /*Create UDP socket*/
    sendersocket = socket(PF_INET, SOCK_DGRAM, 0);

    /*Configure settings in sender struct*/
    sender.sin_family = AF_INET;
    sender.sin_port = htons(port[0]);
    sender.sin_addr.s_addr = inet_addr(ip[0]);
    memset(sender.sin_zero, '\0', sizeof(sender.sin_zero));

    /*bind socket*/
    bind(sendersocket, (struct sockaddr *)&sender, sizeof(sender));

    /*Configure settings in agent struct*/
    agent.sin_family = AF_INET;
    agent.sin_port = htons(port[1]);
    agent.sin_addr.s_addr = inet_addr(ip[1]);
    memset(agent.sin_zero, '\0', sizeof(agent.sin_zero));

    /*Initialize size variable to be used later on*/
    sender_size = sizeof(sender);
    agent_size = sizeof(agent);

    int segment_size, index = 1;

    struct timeval tv = {0, 50000};
    if (setsockopt(sendersocket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
        perror("Error");
    }

    // server
    int imgSize;
    Mat imgServer;
    VideoCapture *cap = new VideoCapture(videoname);
    // get the resolution of the video
    int width = cap->get(CV_CAP_PROP_FRAME_WIDTH);
    int height = cap->get(CV_CAP_PROP_FRAME_HEIGHT);
    //allocate container to load frames
    imgServer = Mat::zeros(height, width, CV_8UC3);
    // ensure the memory is continuous (for efficiency issue.)
    if (!imgServer.isContinuous())
    {
        imgServer = imgServer.clone();
    }
    imgSize = imgServer.total() * imgServer.elemSize();
    //printf("w %d, h %d, imgsize %d\n", width, height, imgSize);
    while (1)
    {
        memset(&Packet, 0, sizeof(Packet));
        Packet.head.fin = 1;
        Packet.head.seqNumber = index;
        sprintf(Packet.data, "%d %d", width, height);
        segment_size = sendto(sendersocket, &Packet, sizeof(segment), 0, (struct sockaddr *)&agent, agent_size);
        printf("send	data	#%d,        winSize = %d\n", index, windowSize);
        memset(&Packet, 0, sizeof(Packet));
        segment_size = recvfrom(sendersocket, &Packet, sizeof(segment), 0, (struct sockaddr *)&agent, &agent_size);
        printf("get     ack	#%d\n", index);
        memset(&Packet, 0, sizeof(Packet));
        ++index;
        ++windowSize;
        if (segment_size > 0)
            break;
    }

    *cap >> imgServer;
    uchar buf[imgSize];
    memcpy(buf, imgServer.data, imgSize);
    int leftSize = imgSize;
    uchar *ptr = buf;

    while (1)
    {
        for (int i = 0; i < windowSize; ++i)
        {
            if (!ResendPKT.empty())
            {
                Packet = ResendPKT.front();
                index = Packet.head.seqNumber;

                ResendPKT.pop();

                Packet.head.last = (i == windowSize - 1) ? 1 : Packet.head.last;

                SentPKT.push(Packet);
                sendto(sendersocket, &Packet, sizeof(segment), 0, (struct sockaddr *)&agent, agent_size);
                printf("rsend	data	#%d,    windowSize = %d\n", index, windowSize);
                ++index;
            }
            else if (leftSize >= datasize) //如果frame中剩下的沒傳出的data大於一個segment可以傳的大小
            {
                memcpy(Packet.data, ptr, sizeof(Packet.data));
                Packet.head.seqNumber = index;

                ptr += datasize;
                leftSize -= datasize;

                Packet.head.last = (i == windowSize - 1) ? 1 : Packet.head.last;

                sendto(sendersocket, &Packet, sizeof(segment), 0, (struct sockaddr *)&agent, agent_size);
                printf("send	data	#%d,    windowSize = %d\n", index, windowSize);
                SentPKT.push(Packet); //已經送了這個Packet的packet
                memset(&Packet, 0, sizeof(Packet));
                ++index;
            }
            else //leftSIze<datasize
            {
                memcpy(Packet.data, ptr, leftSize);
                Packet.head.seqNumber = index;

                Packet.head.last = (i == windowSize - 1) ? 1 : Packet.head.last;

                sendto(sendersocket, &Packet, sizeof(segment), 0, (struct sockaddr *)&agent, agent_size);
                printf("send	data	#%d,    windowSize = %d\n", index, windowSize);
                SentPKT.push(Packet);
                memset(&Packet, 0, sizeof(Packet));
                ++index;

                //int ending = cap->read(imgServer);//
                if (!(cap->read(imgServer))) //讀下一張frame，若沒有影片了
                {
                    Packet.head.seqNumber = index;
                    Packet.head.fin = 1;
                    sendto(sendersocket, &Packet, sizeof(segment), 0, (struct sockaddr *)&agent, agent_size);
                    printf("send	fin\n");
                    memset(&Packet, 0, sizeof(Packet));
                    recvfrom(sendersocket, &Packet, sizeof(segment), 0, (struct sockaddr *)&agent, &agent_size);
                    printf("recv    finack\n");
                    return 0;
                }
                memcpy(buf, imgServer.data, imgSize);
                leftSize = imgSize;
                ptr = buf;
            }
        }

        int get = 0, last_ack;
        for (int i = 0; i < windowSize; ++i)
        {
            if (recvfrom(sendersocket, &Packet, sizeof(segment), 0, (struct sockaddr *)&agent, &agent_size) > 0)
            {
                last_ack = Packet.head.ackNumber;
                printf("get     ack	#%d\n", Packet.head.ackNumber);
                memset(&Packet, 0, sizeof(Packet));
            }
        }
        if (index != last_ack + 1) //上面有ack沒收到
        {
            threshold = MAX(windowSize / 2, 1);
            printf("time    out,            threshold = %d\n", threshold);
            windowSize = 1;
            index = last_ack + 1;
        }
        else //PKT都有收到，調整winSize
        {
            windowSize = (windowSize > threshold) ? windowSize + 1 : windowSize * 2;
        }

        while (1)
        {
            if (ResendPKT.empty() && SentPKT.empty())
            {
                break;
            }
            else if (!ResendPKT.empty() && SentPKT.empty())
            {
                TTTTTTTT.push(ResendPKT.front());
                ResendPKT.pop();
            }
            else if (ResendPKT.empty() && !SentPKT.empty())
            {
                TTTTTTTT.push(SentPKT.front());
                SentPKT.pop();
            }
            else
            {
                if (ResendPKT.front().head.seqNumber < SentPKT.front().head.seqNumber)
                {
                    TTTTTTTT.push(ResendPKT.front());
                    ResendPKT.pop();
                }
                else if (ResendPKT.front().head.seqNumber > SentPKT.front().head.seqNumber)
                {
                    TTTTTTTT.push(SentPKT.front());
                    SentPKT.pop();
                }
                else
                {
                    TTTTTTTT.push(ResendPKT.front());
                    ResendPKT.pop();
                    SentPKT.pop();
                }
            }
        }
        ResendPKT = TTTTTTTT;
        while (!SentPKT.empty())
            SentPKT.pop();
        while (!TTTTTTTT.empty())
            TTTTTTTT.pop();
        while (!ResendPKT.empty())
        {
            if (ResendPKT.front().head.seqNumber < index)
            {
                ResendPKT.pop();
            }
            else
                break;
        }
        /*
        while (!SentPKT.empty() && SentPKT.front() >= index)
        {
            ResendPKT.push(SentPKT.front());
        }
        sort()*/
    }
    ////////////////////////////////////////////////////
    cap->release();
    return 0;
}