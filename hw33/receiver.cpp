#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include "opencv2/opencv.hpp"

#define BUFFSIZE 32

using namespace cv;

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
	char data[4096];
} segment;

void setIP(char *dst, char *src) {
    if(strcmp(src, "0.0.0.0") == 0 || strcmp(src, "local") == 0
            || strcmp(src, "localhost")) {
        sscanf("127.0.0.1", "%s", dst);
    } else {
        sscanf(src, "%s", dst);
    }
}

int main(int argc, char* argv[])
{
    int receiversocket, portNum, nBytes;
    char videoname[1000];
    segment s_tmp;
    struct sockaddr_in agent, receiver;
    socklen_t agent_size, recv_size;
    char ip[2][50];
    int port[2], i;
    
    if(argc != 5)
    {
        fprintf(stderr,"用法: %s <agent IP> <recv IP> <agent port> <recv port>\n", argv[0]);
        fprintf(stderr, "例如: ./receiver 127.0.0.1 127.0.0.1 8888 8889\n");
        exit(1);
    }
    else
    {
        setIP(ip[0], argv[1]);
        setIP(ip[1], argv[2]);

        sscanf(argv[3], "%d", &port[0]);
        sscanf(argv[4], "%d", &port[1]);
    }

    /*Create UDP socket*/
    receiversocket = socket(PF_INET, SOCK_DGRAM, 0);

    /*Configure settings in agent struct*/
    agent.sin_family = AF_INET;
    agent.sin_port = htons(port[0]);
    agent.sin_addr.s_addr = inet_addr(ip[0]);
    memset(agent.sin_zero, '\0', sizeof(agent.sin_zero));

    /*Configure settings in receiver struct*/
    receiver.sin_family = AF_INET;
    receiver.sin_port = htons(port[1]);
    receiver.sin_addr.s_addr = inet_addr(ip[1]);
    memset(receiver.sin_zero, '\0', sizeof(receiver.sin_zero));

    /*bind socket*/
    bind(receiversocket,(struct sockaddr *)&receiver,sizeof(receiver)); 

    /*Initialize size variable to be used later on*/
    agent_size = sizeof(agent);
    recv_size = sizeof(receiver);

    int segment_size, index = 1;

    // client
    Mat imgClient;
    // get the resolution of the video
    char resolution[4096] = {0};

    segment_size = recvfrom(receiversocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&agent, &agent_size);
    printf("recv	data	#%d\n",index);
    sprintf(resolution,"%s",s_tmp.data);
    memset(&s_tmp, 0, sizeof(s_tmp));
    s_tmp.head.ack = 1;
    s_tmp.head.ackNumber = index;
    sendto(receiversocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&agent, agent_size);
    printf("send     ack	#%d\n", index);
    memset(&s_tmp, 0, sizeof(s_tmp));
    index++;

    char *w = strtok(resolution," ");
    char *h = strtok(NULL," ");
    int width = atoi(w);
    int height = atoi(h);
    //allocate container to load frames
    imgClient = Mat::zeros(height, width, CV_8UC3);
    // ensure the memory is continuous (for efficiency issue.)
    if (!imgClient.isContinuous()) {
        imgClient = imgClient.clone();
    }
    int imgSize = imgClient.total() * imgClient.elemSize();
    printf("w %d, h %d, imgsize %d\n", width, height, imgSize);

    char save[32][4096];
    uchar buf[imgSize];
    int leftSize = imgSize;
    uchar *ptr = buf;
    while(1) {
        for (int i = 0; i < BUFFSIZE; ++i) {
            recvfrom(receiversocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&agent, &agent_size);
            //printf("%d %d\n", s_tmp.head.seqNumber, index);
            if (s_tmp.head.fin == 1) {
                printf("recv	fin\n");
                memset(&s_tmp, 0, sizeof(s_tmp));
                s_tmp.head.ack = 1;
                s_tmp.head.fin = 1;
                sendto(receiversocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&agent, agent_size);
                printf("send    finack\n");
            }
            else if (s_tmp.head.seqNumber == index) {
                printf("recv	data	#%d\n", index);
                memcpy(save[i], s_tmp.data, sizeof(s_tmp.data));
                memset(&s_tmp, 0, sizeof(s_tmp));
                s_tmp.head.ack = 1;
                s_tmp.head.ackNumber = index;
                sendto(receiversocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&agent, agent_size);
                printf("send    ack     #%d\n", index);
                ++index;
            }
            else {
                printf("drop    data    #%d\n", s_tmp.head.seqNumber);
                s_tmp.head.ack = 1;
                s_tmp.head.ackNumber = index-1;
                sendto(receiversocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&agent, agent_size);
                --i;
                continue;
            }
        }
        --index;
        while (1) {
            recvfrom(receiversocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&agent, &agent_size);
            //if (s_tmp.head.seqNumber > 35) printf("          %d %d\n", s_tmp.head.seqNumber, index);
            printf("drop    data    #%d\n", s_tmp.head.seqNumber);
            int last = s_tmp.head.last;
            memset(&s_tmp, 0, sizeof(s_tmp));
            s_tmp.head.ack = 1;
            s_tmp.head.ackNumber = index;
            sendto(receiversocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&agent, agent_size);
            printf("send    ack     #%d\n", index);
            if (last == 1) {
                ++index;
                break;
            }
        }

        for (int i = 0; i < 32; ++i) {
            if (leftSize >= sizeof(save[i])) {
                memcpy(ptr, save[i], sizeof(save[i]));
                leftSize -= sizeof(save[i]);
                ptr += sizeof(save[i]);
            }
            else {
                memcpy(ptr, save[i], leftSize);
                uchar *iptr = imgClient.data;
                memcpy(iptr,buf,imgSize);

                imshow("Video", imgClient);
                //Press ESC on keyboard to exit
                // notice: this part is necessary due to openCV's design.
                // waitKey means a delay to get the next frame.
                char c = (char)waitKey(3.3333);
                if(c==27)
                {
                    break;
                }
                memset(&buf, 0, sizeof(buf));
                ptr = buf;
                leftSize = imgSize;
            }
        }
        memset(&save, 0, sizeof(save));
    }
    ////////////////////////////////////////////////////
	destroyAllWindows();
    return 0;
}