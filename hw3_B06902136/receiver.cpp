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

typedef struct
{
    int length;
    int seqNumber;
    int ackNumber;
    int fin;
    int syn;
    int ack;
} header;

typedef struct
{
    header head;
    char data[1000];
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
    int receiversocket, portNum, nBytes;
    char videoname[1000];
    segment s_tmp;
    struct sockaddr_in agent, receiver, tmp_addr;
    socklen_t agent_size, recv_size, tmp_size;
    char ip[2][50];
    int port[2], i;

    if (argc != 5)
    {
        fprintf(stderr, "用法: %s <agent IP> <recv IP> <agent port> <recv port>\n", argv[0]);
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
    bind(receiversocket, (struct sockaddr *)&receiver, sizeof(receiver));

    /*Initialize size variable to be used later on*/
    agent_size = sizeof(agent);
    recv_size = sizeof(receiver);

    int segment_size, index = 0;

    // client
    Mat imgClient;
    // get the resolution of the video
    char resolution[1000] = {0};
    while (1)
    {
        segment_size = recvfrom(receiversocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&agent, &agent_size);
        if (segment_size > 0)
        {
            printf("recv	data	#%d\n", index);
            sprintf(resolution, "%s", s_tmp.data);
            memset(&s_tmp, 0, sizeof(s_tmp));
            s_tmp.head.ack = 1;
            s_tmp.head.ackNumber = index;
            sendto(receiversocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&agent, agent_size);
            printf("send     ack	#%d\n", index);
            memset(&s_tmp, 0, sizeof(s_tmp));
            index++;
            break;
        }
    }
    //char *w = strtok(resolution," ");
    //char *h = strtok(NULL," ");
    int width = atoi(strtok(resolution, " "));
    int height = atoi(strtok(NULL, " "));

    //allocate container to load frames
    imgClient = Mat::zeros(height, width, CV_8UC3);
    // ensure the memory is continuous (for efficiency issue.)
    if (!imgClient.isContinuous())
    {
        imgClient = imgClient.clone();
    }
    int imgSize = imgClient.total() * imgClient.elemSize();
    cout << "imgSize: " << imgSize << endl;
    while (1)
    {
        // get the size of a frame in bytes
        /*
        while (1)
        {
            segment_size = recvfrom(receiversocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&agent, &agent_size);
            if (segment_size > 0)
            {
                printf("recv	data	#%d\n", index);
                cout << "recv size: " << segment_size << endl;
                imgSize = atoi(s_tmp.data);
                memset(&s_tmp, 0, sizeof(s_tmp));
                s_tmp.head.ack = 1;
                s_tmp.head.ackNumber = index;
                sendto(receiversocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&agent, agent_size);
                printf("send     ack	#%d\n", index);
                memset(&s_tmp, 0, sizeof(s_tmp));
                index++;
                break;
            }
        }*/

        // allocate a buffer to load the frame (there would be 2 buffers in the world of the Internet)
        uchar buffer[imgSize];
        // get the frame
        int leftSize = imgSize;
        uchar *ptr = buffer;
        while (leftSize > 0)
        {
            segment_size = recvfrom(receiversocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&agent, &agent_size);
            if (segment_size > 0)
            {
                if (s_tmp.head.seqNumber == index)
                {
                    printf("recv	data	#%d\n", index);
                    memcpy(ptr, s_tmp.data, sizeof(s_tmp.data));
                    int recvSize = sizeof(s_tmp.data);
                    memset(&s_tmp, 0, sizeof(s_tmp));
                    s_tmp.head.ack = 1;
                    s_tmp.head.ackNumber = index;
                    sendto(receiversocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&agent, agent_size);
                    printf("send     ack	#%d\n", index);
                    memset(&s_tmp, 0, sizeof(s_tmp));
                    ptr += recvSize;
                    leftSize -= recvSize;
                    index++;
                }
                else
                {
                    printf("recv	data	#%d\n", index);
                    memset(&s_tmp, 0, sizeof(s_tmp));
                    s_tmp.head.ack = 1;
                    s_tmp.head.ackNumber = index;
                    sendto(receiversocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&agent, agent_size);
                    printf("send     ack	#%d\n", index);
                    memset(&s_tmp, 0, sizeof(s_tmp));
                }
                break;
            }
        }

        // copy a fream from buffer to the container on client
        uchar *iptr = imgClient.data;
        memcpy(iptr, buffer, imgSize);
        startWindowThread();
        imshow("Video", imgClient);
        //Press ESC on keyboard to exit
        // notice: this part is necessary due to openCV's design.
        // waitKey means a delay to get the next frame.
        char c = (char)waitKey(33.3333);
        if (c == 27)
        {
            break;
        }
    }
    ////////////////////////////////////////////////////
    destroyAllWindows();
    return 0;
}
