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
    char data[4096];
} segment;

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
    segment s_tmp;
    struct sockaddr_in sender, agent;
    socklen_t sender_size, agent_size;
    char ip[2][50];
    int port[2], i;

    int ws = 1, trs = 16;

    if (argc != 6)
    {
        fprintf(stderr, "用法: %s <sender IP> <agent IP> <sender port> <agent port> <videoname>\n", argv[0]);
        fprintf(stderr, "例如: ./sender 127.0.0.1 127.0.0.1 8887 8888 tmp.mpg\n");
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
    printf("w %d, h %d, imgsize %d\n", width, height, imgSize);

    memset(&s_tmp, 0, sizeof(s_tmp));
    s_tmp.head.fin = 1;
    s_tmp.head.seqNumber = index;
    sprintf(s_tmp.data, "%d %d", width, height);
    segment_size = sendto(sendersocket, &s_tmp, sizeof(segment), 0, (struct sockaddr *)&agent, agent_size);
    printf("send	data	#%d,        winSize = %d\n", index, ws);
    memset(&s_tmp, 0, sizeof(s_tmp));
    segment_size = recvfrom(sendersocket, &s_tmp, sizeof(segment), 0, (struct sockaddr *)&agent, &agent_size);
    printf("get     ack	#%d\n", index);
    memset(&s_tmp, 0, sizeof(s_tmp));
    ++index;
    ++ws;

    *cap >> imgServer;
    uchar buf[imgSize];
    memcpy(buf, imgServer.data, imgSize);
    int leftSize = imgSize;
    uchar *ptr = buf;
    queue<segment> tmp, tmp2, tmp3;
    while (1)
    {
        for (int i = 0; i < ws; ++i)
        {
            if (!tmp.empty())
            {
                s_tmp = tmp.front();
                index = s_tmp.head.seqNumber;
                //printf("index = %d\n", index);
                tmp.pop();
                if (i == ws - 1)
                {
                    s_tmp.head.last = 1;
                }
                tmp2.push(s_tmp);
                sendto(sendersocket, &s_tmp, sizeof(segment), 0, (struct sockaddr *)&agent, agent_size);
                printf("send	data	#%d,\twinSize = %d\n", index, ws);
                ++index;
            }
            else if (leftSize >= 4096)
            {
                memcpy(s_tmp.data, ptr, sizeof(s_tmp.data));
                s_tmp.head.seqNumber = index;
                int sentSize = sizeof(s_tmp.data);

                ptr += 4096;
                leftSize -= 4096;
                if (i == ws - 1)
                {
                    s_tmp.head.last = 1;
                }

                sendto(sendersocket, &s_tmp, sizeof(segment), 0, (struct sockaddr *)&agent, agent_size);
                printf("send	data	#%d,\t\twinSize = %d\n", index, ws);
                tmp2.push(s_tmp);
                memset(&s_tmp, 0, sizeof(s_tmp));
                ++index;
            }
            else
            {
                memcpy(s_tmp.data, ptr, leftSize);
                s_tmp.head.seqNumber = index;

                if (i == ws - 1)
                {
                    s_tmp.head.last = 1;
                }

                sendto(sendersocket, &s_tmp, sizeof(segment), 0, (struct sockaddr *)&agent, agent_size);
                printf("send	data	#%d,\twinSize = %d\n", index, ws);
                tmp2.push(s_tmp);
                memset(&s_tmp, 0, sizeof(s_tmp));
                ++index;

                int result = cap->read(imgServer);
                if (!result)
                {
                    s_tmp.head.seqNumber = index;
                    s_tmp.head.fin = 1;
                    sendto(sendersocket, &s_tmp, sizeof(segment), 0, (struct sockaddr *)&agent, agent_size);
                    printf("send	fin\n");
                    memset(&s_tmp, 0, sizeof(s_tmp));
                    recvfrom(sendersocket, &s_tmp, sizeof(segment), 0, (struct sockaddr *)&agent, &agent_size);
                    printf("recv    finack\n");
                    return 0;
                }
                memcpy(buf, imgServer.data, imgSize);
                leftSize = imgSize;
                ptr = buf;
            }
        }

        int get = 0, last_ack;
        for (int i = 0; i < ws; ++i)
        {
            if (recvfrom(sendersocket, &s_tmp, sizeof(segment), 0, (struct sockaddr *)&agent, &agent_size) > 0)
            {
                last_ack = s_tmp.head.ackNumber;
                printf("get     ack	#%d\n", s_tmp.head.ackNumber);
                memset(&s_tmp, 0, sizeof(s_tmp));
            }
        }
        if (index != last_ack + 1)
        {
            trs = MAX(ws / 2, 1);
            printf("time\tout,\t\t\tthreshold = %d\n", trs);
            ws = 1;
        }
        else
        {
            ws = (ws > trs) ? ws + 1 : ws * 2;
        }
        index = last_ack + 1;
        segment aa, bb;
        while (1)
        {
            if (tmp.empty() && tmp2.empty())
            {
                break;
            }
            else if (!tmp.empty() && tmp2.empty())
            {
                aa = tmp.front();
                tmp.pop();
                tmp3.push(aa);
            }
            else if (tmp.empty() && !tmp2.empty())
            {
                aa = tmp2.front();
                tmp2.pop();
                tmp3.push(aa);
            }
            else
            {
                aa = tmp.front();
                bb = tmp2.front();
                if (aa.head.seqNumber < bb.head.seqNumber)
                {
                    tmp3.push(aa);
                    tmp.pop();
                }
                else if (aa.head.seqNumber > bb.head.seqNumber)
                {
                    tmp3.push(bb);
                    tmp2.pop();
                }
                else
                {
                    tmp3.push(aa);
                    tmp.pop();
                    tmp2.pop();
                }
            }
        }
        tmp = tmp3;
        while (!tmp2.empty())
            tmp2.pop();
        while (!tmp3.empty())
            tmp3.pop();
        while (!tmp.empty())
        {
            //printf("%d!!!  ", index);
            if (tmp.front().head.seqNumber < index)
            {
                //printf("%d  ", tmp.front().head.seqNumber);
                tmp.pop();
            }
            else
                break;
        }
    }
    ////////////////////////////////////////////////////
    cap->release();
    return 0;
}