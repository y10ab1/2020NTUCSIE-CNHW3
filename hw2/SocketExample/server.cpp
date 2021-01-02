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

                        /*check commands*/
                        if (strncmp("ls", receiveMessage, 2) == 0)
                        {
                            cout << "recived: " << receiveMessage << "\n";
                            status[i] = 1;
                            ff[i].open("list.txt", ios::in);
                        }
                        else if (strncmp("play", receiveMessage, 4) == 0)
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
                        else if (strncmp("put", receiveMessage, 3) == 0)
                        {
                            status[i] = 3;
                            cnt_count[i] = 0;
                            char put_filename[100][BUFF_SIZE] = {};
                            bzero(put_filename[i], sizeof(char) * BUFF_SIZE);
                            if ((recved = recv(remoteSocket[i], put_filename[i], sizeof(char) * BUFF_SIZE, 0)) < 0)
                            {
                                cout << "Socket: " << remoteSocket[i] << " i: " << i << " has commands but fail\n";
                                cout << "recv failed in get, with received bytes = " << recved << endl;
                                break;
                            }
                            string file_put[100];
                            file_put[i] = folderPath + "/" + put_filename[i];

                            f_put[i].open(file_put[i].c_str(), ios::out | ios::binary);
                            sleep(1);
                            char ch[100][BUFF_SIZE] = {};
                            if ((recved = recv(remoteSocket[i], ch[i], BUFF_SIZE, 0)) < 0)
                            {
                                cout << "Socket: " << remoteSocket[i] << " i: " << i << " has commands but fail\n";
                                cout << "recv failed in get, with received bytes = " << recved << endl;
                                break;
                            }
                            PUT_FILESIZE[i] = atoi(ch[i]);
                            cout << "FILESIZE is: " << PUT_FILESIZE[i] << endl;
                        }
                        else if (strncmp("get", receiveMessage, 3) == 0)
                        {
                            countt[i] = 0;
                            bzero(receiveMessage, sizeof(char) * BUFF_SIZE);
                            if ((recved = recv(remoteSocket[i], receiveMessage, sizeof(char) * BUFF_SIZE, 0)) < 0)
                            {
                                cout << "Socket: " << remoteSocket[i] << " i: " << i << " has commands but fail\n";
                                cout << "recv failed in get, with received bytes = " << recved << endl;
                                break;
                            }
                            string filee[100];
                            filee[i] = folderPath + "/" + receiveMessage;
                            cout << filee[i] << endl;

                            if (access(filee[i].c_str(), F_OK) < 0)
                            {
                                cout << "The file doesn’t exist." << endl;
                                break;
                            }

                            status[i] = 4;
                            fstream ffs[100];
                            ffs[i].open(filee[i].c_str(), ios::in | ios::binary);
                            ffs[i].seekg(0, ios::end);
                            filesize[i] = ffs[i].tellg();
                            ffs[i].seekg(0, ios::beg);
                            ffs[i].close();
                            ff[i].open(filee[i].c_str(), ios::in | ios::binary);
                            ff[i].seekg(0, ios::beg);
                            cout << "FILE SIZE= " << filesize[i] << endl;
                            stringstream sn[100];
                            sn[i] << filesize[i];
                            string fs[100];
                            sn[i] >> fs[i];
                            send(remoteSocket[i], fs[i].c_str(), sizeof(fs[i].c_str()), 0);
                            sleep(1);
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
                    /* ls */ {
                        cout << "executing ls:\n";

                        system("ls ./ServerFolder > list.txt");

                        string s;
                        char msg[BUFF_SIZE] = {};
                        ff[i] >> msg;
                        if ((msg[0] == '\0') || (sent = send(remoteSocket[i], msg, sizeof(msg), 0)) < 0)
                        {

                            cerr << "bytes = " << sent << endl;
                            cout << "sock num: " << remoteSocket[i] << endl;
                            status[i] = 0;

                            cout << "end of ls\n";
                            ff[i].seekg(0, ios::beg);
                            ff[i].close();
                        }
                        cout << msg << endl;
                    }

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
                case 3:
                { /* put */
                    cout << "executing put\n";

                    tv.tv_sec = 3;
                    tv.tv_usec = 0;
                    int newrv = select(fdmax + 1, &time_socks, NULL, NULL, &tv);
                    char ch[100][BUFF_SIZE + 5] = {};
                    bool end[100] = {0};
                    if (newrv == 0)
                    {
                        cout << "timeout, newrv= " << newrv << endl;
                        status[i] = 0;
                        f_put[i].close();
                        cout << "end of put\n";
                        break;
                    }
                    else if (end[i] == 1 || (recved = recv(remoteSocket[i], ch[i], BUFF_SIZE, MSG_WAITALL)) == -1)
                    {
                        cerr << "recv failed or end, received bytes = " << recved << endl;
                        status[i] = 0;
                        f_put[i].close();
                        cout << "end of put\n";
                        break;
                    }
                    cout << "cnt_count: " << cnt_count[i] << endl;
                    cout << ch[i] << endl;
                    for (int cnt = 0; cnt < 1024 && ((cnt + cnt_count[i]) < PUT_FILESIZE[i]);)
                    {
                        f_put[i].write(&ch[i][cnt++], 1);
                        f_put[i].flush();
                        if ((cnt + cnt_count[i]) == PUT_FILESIZE[i] - 1)
                        {
                            end[i] = 1;
                        }
                    }
                    cnt_count[i] += 1024;
                }

                break;
                case 4:
                    /* get */ {
                        cout << "executing get\n";

                        char ch[100][BUFF_SIZE + 5] = {};

                        bool get[100] = {0};

                        if (((countt[i]++) * BUFF_SIZE) < filesize[i])
                        {

                            ff[i].read(ch[i], BUFF_SIZE);
                        }
                        else
                        {
                            get[i] = 1;
                        }

                        cout << ch[i] << endl;

                        tv.tv_sec = 3;
                        tv.tv_usec = 0;
                        int newrv = select(remoteSocket[i] + 1, NULL, &time_socks, NULL, &tv);
                        if (newrv == 0)
                        {
                            cout << "timeout, newrv= " << newrv << endl;
                            status[i] = 0;

                            cout << "end of get\n";
                            ff[i].close();
                            break;
                        }
                        else if (get[i] == 1 || (sent = send(remoteSocket[i], ch[i], BUFF_SIZE, 0)) < 0)
                        {

                            cerr << "bytes = " << sent << endl;
                            cout << "get: " << get[i] << endl;
                            cout << "sock num: " << remoteSocket[i] << endl;
                            status[i] = 0;

                            cout << "end of get\n";
                            ff[i].close();
                        }

                        cerr << "bytes = " << sent << endl;
                        cout << "get: " << get[i] << endl;
                        cout << "count = " << countt[i] << endl;
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
