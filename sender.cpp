#include <iostream>
#include <sstream>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sstream>
#include <fstream>

#include "opencv2/opencv.hpp"

#define BUFF_SIZE 1024


typedef struct {
	int length;
	int seqNumber;
	int ackNumber;
	int fin;
	int syn;
	int ack;
} header;

typedef struct{
	header head;
	char data[1000];
} segment;

using namespace std;
using namespace cv;
VideoCapture cap[100];
int countt[100] = {0};
Mat imgServer;
string filename[100];
int imgSize;
fstream ff[100];
fstream f_put[100];
long PUT_FILESIZE[100] = {0};
long long filesize[100] = {0};
int cnt_count[100] = {0};
int main(int argc, char **argv)
{

    /*setting start*/
    int localSocket, port = 4097;
    int remoteSocket[100] = {0};
    int fdmax = 0;
    fd_set master_socks, command_socks, time_socks;
    FD_ZERO(&master_socks);
    FD_ZERO(&command_socks);
    int status[100] = {0};

    struct timeval tv;

    tv.tv_sec = 0;
    tv.tv_usec = 1;

    if (argc < 2)
    {
        cout << "Command not found.\n";
    }
    else
    {
        port = atoi(argv[1]);
    }

    string defaultPath = "./";
    string folderPath = defaultPath + "ServerFolder";

    if (0 != access(folderPath.c_str(), 0))
    {
        // if this folder not exist, create a new one.
        mkdir(folderPath.c_str(), 0777);
    }

    int recved;

    struct sockaddr_in localAddr, remoteAddr;

    int addrLen = sizeof(struct sockaddr_in);

    localSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (localSocket == -1)
    {
        printf("socket() call failed!!");
        return 0;
    }

    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = INADDR_ANY;
    localAddr.sin_port = htons(port);

    char Message[BUFF_SIZE] = {};

    if (bind(localSocket, (struct sockaddr *)&localAddr, sizeof(localAddr)) < 0)
    {
        printf("Can't bind() socket");
        return 0;
    }

    listen(localSocket, 3);
    FD_SET(localSocket, &master_socks);
    fdmax = localSocket;
    cout << "Connection bind: " << localSocket << endl;

    /*setting end*/

    while (1)
    {
        command_socks = master_socks;

        //todo : select
        //fdmax = (remoteSocket > fdmax) ? remoteSocket : fdmax;

        tv.tv_sec = 0;
        tv.tv_usec = 1;

        select(fdmax + 1, &command_socks, NULL, NULL, &tv);

        int sent;
        //std::cout << "Waiting for connections...\n"
        //          << "Server Port:" << port << std::endl;

        for (int i = 0; i <= fdmax; i++)
        {
            if (FD_ISSET(i, &command_socks) && status[i] != 3)
            {
                cout << "i: " << i << " has command\n";
                if (i == localSocket)
                { //new socket
                    int newsockfd;
                    if ((newsockfd = accept(localSocket, (struct sockaddr *)&remoteAddr, (socklen_t *)&addrLen)) != -1)
                    {
                        remoteSocket[newsockfd] = newsockfd;
                        FD_SET(remoteSocket[newsockfd], &master_socks);
                        fdmax = (remoteSocket[newsockfd] > fdmax) ? remoteSocket[newsockfd] : fdmax;
                        cout << "Connection accepted: " << remoteSocket[newsockfd] << endl;
                        cout << "fdmax: " << fdmax << endl;
                    }
                    else
                    {
                        perror("accept failed!\n");
                        return 0;
                    }
                }
                else
                {

                    /*receive command*/
                    char receiveMessage[BUFF_SIZE] = {};

                    bzero(receiveMessage, sizeof(char) * BUFF_SIZE);
                    if ((recved = recv(remoteSocket[i], receiveMessage, sizeof(char) * BUFF_SIZE, 0)) < 0)
                    {
                        cout << "Socket: " << remoteSocket[i] << " i: " << i << " has commands but fail\n";
                        cout << "recv failed, with received bytes = " << recved << endl;
                        break;
                    }
                    else if (recved == 0)
                    {
                        cout << "<socket: " << remoteSocket[i] << " closed>\n";
                        FD_CLR(remoteSocket[i], &master_socks);
                        break;
                    }
                    else
                    {
                        printf("word len, command: %d: %s\n", recved, receiveMessage);

                        if (strncmp("play", receiveMessage, 4) == 0)
                        {
                            status[i] = 2;

                            // server

                            bzero(receiveMessage, sizeof(char) * BUFF_SIZE);

                            recv(remoteSocket[i], receiveMessage, sizeof(char) * BUFF_SIZE, 0);
                            cout << "videoname: " << receiveMessage << "\n";

                            string filee[100];
                            filee[i] = folderPath + "/" + receiveMessage;
                            cout << filee[i] << endl;

                            cap[i].open(filee[i].c_str());

                            // get the resolution of the video
                            int width = cap[i].get(CV_CAP_PROP_FRAME_WIDTH);
                            int height = cap[i].get(CV_CAP_PROP_FRAME_HEIGHT);
                            cout << width << ", " << height << endl;

                            //allocate container to load frames

                            imgServer = Mat::zeros(height, width, CV_8UC3);

                            sprintf(Message, "%d", width);
                            cout << "Message" << Message << endl;
                            sent = send(remoteSocket[i], Message, strlen(Message), 0);
                            bzero(Message, sizeof(char) * BUFF_SIZE);
                            sleep(1);
                            sprintf(Message, "%d", height);
                            sent = send(remoteSocket[i], Message, strlen(Message), 0);
                            imgSize = imgServer.total() * imgServer.elemSize();
                            // ensure the memory is continuous (for efficiency issue.)
                            if (!imgServer.isContinuous())
                            {
                                imgServer = imgServer.clone();
                            }
                        }
                        else
                        {
                            cout << "Command not found.\n";
                        }
                    }
                }
            }

            if (FD_ISSET(i, &master_socks) && status[i] != 0)
            {
                FD_ZERO(&time_socks);
                FD_SET(remoteSocket[i], &time_socks);

                cout << "******executing sth******\n";
                switch (status[i])
                {
                case 1:

                    break;
                case 2:
                    /* play */

                    cout << "executing play:\n";

                    cap[i] >> imgServer;

                    struct timeval timeout;
                    timeout.tv_sec = 3;
                    timeout.tv_usec = 0;
                    if (setsockopt(remoteSocket[i], SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout,
                                   sizeof(timeout)) < 0)
                        cerr << "setsockopt failed\n";
                    if (imgServer.empty() || (sent = send(remoteSocket[i], imgServer.data, imgSize, 0)) < 0)
                    {
                        cerr << "bytes = " << sent << endl;
                        cout << "sock num: " << remoteSocket[i] << endl;
                        status[i] = 0;
                        cap[i].release();
                        cout << "end of video\n";
                    }

                    break;

                default:
                    break;
                }
            }
        }
    }
    return 0;
}
