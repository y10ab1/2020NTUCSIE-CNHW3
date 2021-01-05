#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
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
    if(strcmp(src, "0.0.0.0") == 0 || strcmp(src, "local") == 0 || strcmp(src, "localhost")) {
        sscanf("127.0.0.1", "%s", dst);
    } 
    else{
        sscanf(src, "%s", dst);
    }
}

int main(int argc, char* argv[]){
    int receiversocket, portNum, nBytes;
    segment s_tmp;
    struct sockaddr_in agent, receiver;
    socklen_t agent_size, recv_size;
    char ip[2][50];
    int port[2], i;
    
    if(argc != 5){
        fprintf(stderr,"format: %s <agent IP> <recv IP> <agent port> <recv port>\n", argv[0]);
        fprintf(stderr, "example: ./receiver 127.0.0.1 127.0.0.1 3334 3335\n");
        exit(1);
    }
    else{
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

    // init
    int index = 0;

    // resolution
    char resolution[128] = {0};
    recvfrom(receiversocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&agent, &agent_size);
    sprintf(resolution,"%s",s_tmp.data);
    printf("recv\tdata\t#%d\n",index), memset(&s_tmp, 0, sizeof(s_tmp));
    s_tmp.head.ack = 1;
    s_tmp.head.ackNumber = index;
    sendto(receiversocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&agent, agent_size);
    printf("send\tack\t#%d\n", index), memset(&s_tmp, 0, sizeof(s_tmp));

    //create Mat for openCV
    int width = atoi(strtok(resolution," "));
    int height = atoi(strtok(NULL," "));
    Mat imgClient = Mat::zeros(height, width, CV_8UC3);
    if(!imgClient.isContinuous()){
        imgClient = imgClient.clone();
    }
    int imgSize = imgClient.total() * imgClient.elemSize();

    //init
    char packagebuffer[BUFFERSIZE][PACKSIZE];
    int leftsize = imgSize;
    uchar buf[imgSize];
    uchar *ptr = buf;
    while(1){
        for(int i = 0; i < BUFFERSIZE; ++i){
            recvfrom(receiversocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&agent, &agent_size);
            if(s_tmp.head.fin == 1){ //finish
                printf("recv\tfin\n"), memset(&s_tmp, 0, sizeof(s_tmp));
                s_tmp.head.ack = 1, s_tmp.head.ackNumber = ++index, s_tmp.head.fin = 1;
                sendto(receiversocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&agent, agent_size);
                printf("send\tfinack\n"), memset(&s_tmp, 0, sizeof(s_tmp));
            }
            else if(s_tmp.head.seqNumber == index + 1){ //no drop
                memcpy(packagebuffer[i], s_tmp.data, sizeof(s_tmp.data));
                printf("recv\tdata\t#%d\n", s_tmp.head.seqNumber), memset(&s_tmp, 0, sizeof(s_tmp));
                s_tmp.head.ack = 1, s_tmp.head.ackNumber = ++index;
                sendto(receiversocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&agent, agent_size);
                printf("send\tack\t#%d\n", index), memset(&s_tmp, 0, sizeof(s_tmp));
            }
            else{ //drop
                printf("drop\tdata\t#%d\n", s_tmp.head.seqNumber);
                s_tmp.head.ack = 1, s_tmp.head.ackNumber = index;
                sendto(receiversocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&agent, agent_size);
                printf("send\tack\t#%d\n", index), memset(&s_tmp, 0, sizeof(s_tmp));
                --i;
            }
        }
        while(1){ //recv overflow
            recvfrom(receiversocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&agent, &agent_size);
            int flag = s_tmp.head.last;
            printf("drop\tdata\t#%d\n", s_tmp.head.seqNumber), memset(&s_tmp, 0, sizeof(s_tmp));
            s_tmp.head.ack = 1, s_tmp.head.ackNumber = index;
            sendto(receiversocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&agent, agent_size);
            printf("send\tack\t#%d\n", index),  memset(&s_tmp, 0, sizeof(s_tmp));
            if (flag == 1) break;
        }
        int crash = 0; // flag of press ESC to close video 
        // flush to frame
        for(int i = 0; i < 32; ++i){
            if(leftsize >= sizeof(packagebuffer[i])){
                memcpy(ptr, packagebuffer[i], sizeof(packagebuffer[i]));
                leftsize -= sizeof(packagebuffer[i]), ptr += sizeof(packagebuffer[i]);
            }
            else{ //show frame
                memcpy(ptr, packagebuffer[i], leftsize);
                uchar *iptr = imgClient.data;
                memcpy(iptr, buf, imgSize);
                imshow("Video", imgClient);
                // Press ESC on keyboard to exit
                // notice: this part is necessary due to openCV's design.
                // waitKey means a delay to get the next frame.
                char c = (char)waitKey(3.3333);
                if(c == 27){
                    crash = 1;
                    break;
                }
                memset(&buf, 0, sizeof(buf));
                ptr = buf, leftsize = imgSize;
            }
        }
        printf("flush\n");
        memset(&packagebuffer, 0, sizeof(packagebuffer));
        if(crash) break;
    }
	destroyAllWindows();
    return 0;
}