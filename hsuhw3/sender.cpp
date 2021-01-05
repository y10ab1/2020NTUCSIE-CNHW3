#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <vector>
#include <queue>
#include <algorithm>
#include "opencv2/opencv.hpp"
using namespace cv;
using namespace std;
#define BUFFERSIZE 32
#define PACKSIZE 4096

typedef struct {
	int length;
	int seqNumber;
	int ackNumber;
	int fin;
	int syn;
	int ack;
    int last;
} header;

typedef struct{
	header head;
	char data[PACKSIZE];
} segment;

void setIP(char *dst, char *src){
    if(strcmp(src, "0.0.0.0") == 0 || strcmp(src, "local") == 0 || strcmp(src, "localhost")){
        sscanf("127.0.0.1", "%s", dst);
    } 
    else{
        sscanf(src, "%s", dst);
    }
}

int main(int argc, char* argv[]){
    int sendersocket, portNum, nBytes;
    char videoname[128];
    segment s_tmp;
    struct sockaddr_in sender, agent;
    socklen_t sender_size, agent_size;
    char ip[2][50];
    int port[2], i;

    if(argc != 6){
        fprintf(stderr,"format: %s <sender IP> <agent IP> <sender port> <agent port> <videoname>\n", argv[0]);
        fprintf(stderr, "example: ./sender 127.0.0.1 127.0.0.1 3333 3334 tmp.mpg\n");
        exit(1);
    }
    else{
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
    bind(sendersocket,(struct sockaddr *)&sender,sizeof(sender));

    /*Configure settings in agent struct*/
    agent.sin_family = AF_INET;
    agent.sin_port = htons(port[1]);
    agent.sin_addr.s_addr = inet_addr(ip[1]);
    memset(agent.sin_zero, '\0', sizeof(agent.sin_zero));

    /*Initialize size variable to be used later on*/
    sender_size = sizeof(sender);
    agent_size = sizeof(agent);

    // init
    int index = 0, window = 1, thresh = 16;

    // timeout
    struct timeval tv;
    tv.tv_sec = 0, tv.tv_usec = 100000;
    if(setsockopt(sendersocket, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) perror("Error");

    // create Mat for openCV
    VideoCapture *cap = new VideoCapture(videoname);
    int width = cap->get(CV_CAP_PROP_FRAME_WIDTH);
    int height = cap->get(CV_CAP_PROP_FRAME_HEIGHT);
    Mat imgServer = Mat::zeros(height, width, CV_8UC3);
    if(!imgServer.isContinuous()) imgServer = imgServer.clone();
    int imgSize = imgServer.total() * imgServer.elemSize();

    // resolution
    memset(&s_tmp, 0, sizeof(s_tmp));
    s_tmp.head.fin = 1, s_tmp.head.seqNumber = index;
    sprintf(s_tmp.data, "%d %d", width, height);
    sendto(sendersocket, &s_tmp, sizeof(segment), 0, (struct sockaddr *)&agent, agent_size);
    printf("send\tdata\t#%d,\twinSize = %d\n", index, window), memset(&s_tmp, 0, sizeof(s_tmp));
    recvfrom(sendersocket, &s_tmp, sizeof(segment), 0, (struct sockaddr *)&agent, &agent_size);
    printf("get\tack\t#%d\n", index), memset(&s_tmp, 0, sizeof(s_tmp));
    
    // first cap frame and init
    *cap >> imgServer;
    uchar buf[imgSize];
    uchar *ptr = buf;
    memcpy(buf, imgServer.data, imgSize);
    int leftsize = imgSize;
    queue<segment> copyqueue1, copyqueue2;
    while(1){
        // sliding wondow handle
        while(!copyqueue1.empty() && copyqueue1.front().head.seqNumber <= index) copyqueue1.pop();
        while(!copyqueue2.empty()) copyqueue2.pop();
        // process pack in window
        for(int i = 0; i < window; ++i){
            if(!copyqueue1.empty()){ // resend
                s_tmp = copyqueue1.front(), copyqueue1.pop(), copyqueue2.push(s_tmp);;
                index = s_tmp.head.seqNumber;
                if (i == window - 1) s_tmp.head.last = 1;
                else s_tmp.head.last = 0;
                sendto(sendersocket, &s_tmp, sizeof(segment), 0, (struct sockaddr *)&agent, agent_size);
                printf("resnd\tdata\t#%d,\twinSize = %d\n", index, window), memset(&s_tmp, 0, sizeof(s_tmp));
            }
            else if(leftsize >= PACKSIZE){ // first to send and isn't the last pack of the frame
                memcpy(s_tmp.data, ptr, sizeof(s_tmp.data)), s_tmp.head.seqNumber = ++index;
                copyqueue1.push(s_tmp);
                ptr += PACKSIZE, leftsize -= PACKSIZE;
                if (i == window - 1) s_tmp.head.last = 1;
                else s_tmp.head.last = 0;
                sendto(sendersocket, &s_tmp, sizeof(segment), 0, (struct sockaddr *)&agent, agent_size);
                printf("send\tdata\t#%d,\twinSize = %d\n", index, window), memset(&s_tmp, 0, sizeof(s_tmp));
            }
            else{ // first to send and the last pack of the frame
                memcpy(s_tmp.data, ptr, leftsize), s_tmp.head.seqNumber = ++index;
                copyqueue1.push(s_tmp);
                if (i == window - 1) s_tmp.head.last = 1;
                else s_tmp.head.last = 0;
                sendto(sendersocket, &s_tmp, sizeof(segment), 0, (struct sockaddr *)&agent, agent_size);
                printf("send\tdata\t#%d,\twinSize = %d\n", index, window), memset(&s_tmp, 0, sizeof(s_tmp));
                // read new frame
                int newframe = cap->read(imgServer);
                if(!newframe){
                    s_tmp.head.seqNumber = ++index, s_tmp.head.fin = 1;
                    sendto(sendersocket, &s_tmp, sizeof(segment), 0, (struct sockaddr *)&agent, agent_size);
                    printf("send\tfin\n"), memset(&s_tmp, 0, sizeof(s_tmp));
                    recvfrom(sendersocket, &s_tmp, sizeof(segment), 0, (struct sockaddr *)&agent, &agent_size);
                    printf("recv\tfinack\n"), memset(&s_tmp, 0, sizeof(s_tmp));
                    return 0;
                }
                memcpy(buf, imgServer.data, imgSize);
                leftsize = imgSize, ptr = buf;
            }
        }
        // recv ack
        int ret_ack;
        for(int i = 0; i < window; ++i){
            if(recvfrom(sendersocket, &s_tmp, sizeof(segment), 0, (struct sockaddr *)&agent, &agent_size) > 0){
                ret_ack = s_tmp.head.ackNumber;
                printf("get\tack\t#%d\n", s_tmp.head.ackNumber);
                memset(&s_tmp, 0, sizeof(s_tmp));
            }
            else break;
        }
        // if any loss pack
        if(index != ret_ack){
            thresh = MAX(window / 2, 1);
            printf("time\tout,\t\tthreshold = %d\n", thresh);
            window = 1;
        }
        else window = window >= thresh? window + 1 : window * 2;
        // window handling
        while(!copyqueue1.empty()){
            segment temp = copyqueue1.front();
            copyqueue1.pop();
            copyqueue2.push(temp);
        }
        copyqueue1 = copyqueue2;
        // next pack window need to send
        index = ret_ack;    
    }
	cap->release();
    return 0;
}