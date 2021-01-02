#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sstream>
#include <fstream>

#include "opencv2/opencv.hpp"

#define BUFF_SIZE 1024

using namespace std;
using namespace cv;

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

int countt = 0;
string IP;
string PORT;

int main(int argc, char *argv[])
{

    int localSocket, remoteSocket, recved, port;

    struct timeval tv;

    tv.tv_sec = 3;
    tv.tv_usec = 0;

    if (argc != 5)
    {
        fprintf(stderr, "用法: %s <agent IP> <recv IP> <agent port> <recv port>\n", argv[0]);
        fprintf(stderr, "例如: ./receiver 127.0.0.1 127.0.0.1 8888 8889\n");
        exit(1);
    }
    else
    {
        string IPPORT(argv[1]);
        int sep = IPPORT.find(':', 0);
        IP = IPPORT.substr(0, sep);
        PORT = IPPORT.substr(sep + 1, IPPORT.size() - sep);
        port = atoi(PORT.c_str());
    }

    localSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (localSocket == -1)
    {
        printf("Fail to create a socket.\n");
        return 0;
    }

    struct sockaddr_in info;
    bzero(&info, sizeof(info));

    info.sin_family = PF_INET;
    info.sin_addr.s_addr = inet_addr(IP.c_str());
    info.sin_port = htons(port);

    int err = connect(localSocket, (struct sockaddr *)&info, sizeof(info));
    if (err == -1)
    {
        printf("Connection error\n");
        return 0;
    }

    while (1)
    {
        char Message[BUFF_SIZE] = {};
        bzero(Message, sizeof(char) * BUFF_SIZE);
        fd_set master_socks;
        FD_ZERO(&master_socks);
        FD_SET(localSocket, &master_socks);
        char receiveMessage[BUFF_SIZE] = {};
        int sent;
        cout << "Enter commands:\n";
        scanf("%s", Message);

        if (strncmp("play", Message, 4) == 0)
        {
            int QUIT = 0;
        /*
        *    string filename;
        *    cin >> filename; //video file name
        *    if (filename.find(".mpg") == string::npos)
        *    {
        *        cout << "The " << filename << " is not a mpg file." << endl;
        *        continue;
        *    }
        *
        *  sent = send(localSocket, Message, strlen(Message), MSG_WAITALL);
        *  bzero(Message, sizeof(char) * BUFF_SIZE);
        *   sleep(1);
        *    sent = send(localSocket, filename.c_str(), strlen(filename.c_str()), MSG_WAITALL);
        */
            // get the resolution of the video
            Mat imgClient;

            bzero(receiveMessage, sizeof(char) * BUFF_SIZE);
            recved = recv(localSocket, receiveMessage, sizeof(char) * BUFF_SIZE, 0);
            int width = atoi(receiveMessage);

            bzero(receiveMessage, sizeof(char) * BUFF_SIZE);
            recved = recv(localSocket, receiveMessage, sizeof(char) * BUFF_SIZE, 0);
            int height = atoi(receiveMessage);

            cout << width << ", " << height << endl;

            //allocate container to load frames
            imgClient = Mat::zeros(height, width, CV_8UC3);
            int imgSize = imgClient.total() * imgClient.elemSize();
            cout << imgSize << endl;
            // ensure the memory is continuous (for efficiency issue.)
            if (!imgClient.isContinuous())
            {
                imgClient = imgClient.clone();
            }

            while (1)
            {
                tv.tv_sec = 3;
                tv.tv_usec = 0;
                int newrv = select(localSocket + 1, &master_socks, NULL, NULL, &tv);
                cout << newrv << endl;
                if (newrv == 0 || waitKey(33.3333) == 27)
                {
                    cout << "timeout, newrv= " << newrv << endl;
                    destroyAllWindows();
                    break;
                }
                else if ((recved = recv(localSocket, imgClient.data, imgSize, MSG_WAITALL)) == -1)
                {
                    cerr << "recv failed, received bytes = " << recved << endl;
                }

                //cout << "recv byte: " << recved << endl;
                startWindowThread();
                imshow("Video", imgClient);
                //Press ESC on keyboard to exit
                // notice: this part is necessary due to openCV's design.
                // waitKey means a delay to get the next frame.
            }
        }
        else
        {
            cout << "Command not found.\n";
        }
    }

    printf("close Socket\n");
    close(localSocket);
    return 0;
}
