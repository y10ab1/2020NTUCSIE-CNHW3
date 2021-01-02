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

int countt = 0;
string IP;
string PORT;

int main(int argc, char *argv[])
{

    int localSocket, remoteSocket, recved, port;

    struct timeval tv;

    tv.tv_sec = 3;
    tv.tv_usec = 0;

    if (argc < 2)
    {
        cout << "Command not found.\n";
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
    int index_folder = getpid();
    stringstream ss;
    ss << index_folder;
    string defaultPath = "./";
    string Client_index;
    ss >> Client_index;
    string folderPath = defaultPath + "Client_" + Client_index + "_Folder";

    if (0 != access(folderPath.c_str(), 0))
    {
        // if this folder not exist, create a new one.
        mkdir(folderPath.c_str(), 0777);
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

        if (strncmp("ls", Message, 2) == 0)
        {

            sent = send(localSocket, Message, strlen(Message), 0);
            while (1)
            {
                tv.tv_sec = 3;
                tv.tv_usec = 0;
                int newrv = select(localSocket + 1, &master_socks, NULL, NULL, &tv);
                if (newrv == 0)
                {
                    cout << "timeout, newrv= " << newrv << endl;

                    break;
                }
                else if ((recved = recv(localSocket, receiveMessage, sizeof(char) * BUFF_SIZE, 0)) == -1)
                {
                    cerr << "recv failed, received bytes = " << recved << endl;
                }
                cout << receiveMessage << endl;
            }
        }
        else if (strncmp("play", Message, 4) == 0)
        {
            int QUIT = 0;

            string filename;
            cin >> filename; //video file name
            if (filename.find(".mpg") == string::npos)
            {
                cout << "The " << filename << " is not a mpg file." << endl;
                continue;
            }

            sent = send(localSocket, Message, strlen(Message), MSG_WAITALL);
            bzero(Message, sizeof(char) * BUFF_SIZE);
            sleep(1);
            sent = send(localSocket, filename.c_str(), strlen(filename.c_str()), MSG_WAITALL);

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
        else if (strncmp("put", Message, 3) == 0)
        {

            char filename[BUFF_SIZE] = {};
            cin >> filename; //file name
            string file_put = folderPath + "/" + filename;
            int filesize = 0;

            if (access(file_put.c_str(), F_OK) < 0)
            {
                cout << "The " << filename << " doesn’t exist." << endl;
                continue;
            }

            fstream put(file_put.c_str(), ios::in | ios::binary);
            put.seekg(0, ios::end);
            filesize = put.tellg();
            put.seekg(0, ios::beg);

            sent = send(localSocket, Message, strlen(Message), 0);
            sleep(1);
            sent = send(localSocket, filename, strlen(filename), 0);
            sleep(1);
            string SIZE;
            stringstream ss;
            ss << filesize;
            ss >> SIZE;
            sent = send(localSocket, SIZE.c_str(), strlen(SIZE.c_str()), 0);
            sleep(1);
            while (1)
            {
                char ch[BUFF_SIZE + 5] = {};

                bool putt = 0;

                if (((countt++) * BUFF_SIZE) < filesize)
                {

                    put.read(ch, BUFF_SIZE);
                }
                else
                {
                    putt = 1;
                }

                //cout << ch << endl;

                tv.tv_sec = 3;
                tv.tv_usec = 0;
                int newst = select(localSocket + 1, NULL, &master_socks, NULL, &tv);
                if (newst == 0)
                {
                    cout << "timeout, newst= " << newst << endl;

                    cout << "end of put\n";
                    put.close();
                    break;
                }
                else if (putt == 1 || ((sent = send(localSocket, ch, BUFF_SIZE, 0)) < 0))
                {

                    cerr << "bytes = " << sent << endl;
                    cout << "put: " << putt << endl;
                    cout << "end of put\n";
                    put.close();
                    break;
                }
                cout << ch << endl;
                cerr << "bytes = " << sent << endl;
                cout << "put: " << putt << endl;
                cout << "count = " << countt << endl;
            }
            put.seekg(0, ios::beg);
            put.close();
	    countt=0;
        }
        else if (strncmp("get", Message, 3) == 0)
        {
            /*check filename with ls*/
            sent = send(localSocket, "ls", strlen("ls"), 0);

            string listt = folderPath + "/list.txt";
            cout << listt << endl;

            ofstream gete(listt.c_str());
            gete.close();
            fstream getfile(listt.c_str(), ios::out | ios::in);
            getfile.seekg(0, ios::beg);
            while (1)
            {
                tv.tv_sec = 3;
                tv.tv_usec = 0;
                int newrv = select(localSocket + 1, &master_socks, NULL, NULL, &tv);
                if (newrv == 0)
                {
                    cout << "timeout, newrv= " << newrv << endl;

                    break;
                }
                else if ((recved = recv(localSocket, receiveMessage, sizeof(char) * BUFF_SIZE, 0)) == -1)
                {
                    cerr << "recv failed, received bytes = " << recved << endl;
                }

                getfile << receiveMessage << endl;
            }

            long cnt_count = 0;

            char filename[BUFF_SIZE] = {};
            cin >> filename; //file name

            getfile.seekg(0, ios::beg);
            string tmp;
            bool file_exist = 1;
            while (getfile >> tmp)
            {
                cout << tmp << endl;
                if (strncmp(filename, tmp.c_str(), sizeof(tmp.c_str())) == 0)
                {
                    file_exist = 0;
                    break;
                }
            }
            getfile.seekg(0, ios::beg);
            getfile.close();
            if (file_exist)
            {
                cout << "The " << filename << " doesn’t exist." << endl;
                continue;
            }

            FD_ZERO(&master_socks);
            FD_SET(localSocket, &master_socks);

            sent = send(localSocket, Message, strlen(Message), 0);
            sleep(1);
            sent = send(localSocket, filename, strlen(filename), 0);

            string File_path;
            File_path = (folderPath + "/" + filename);

            cout << File_path << endl;
            fstream ff(File_path.c_str(), ios::out | ios::binary);

            sleep(1);
            char filesize[BUFF_SIZE] = {};
            recv(localSocket, filesize, BUFF_SIZE, 0);
            long long FILESIZE = atoi(filesize);
            cout << "get filesize: " << FILESIZE << endl;

            while (1)
            {
                tv.tv_sec = 3;
                tv.tv_usec = 0;
                int newrv = select(localSocket + 1, &master_socks, NULL, NULL, &tv);
                char ch[BUFF_SIZE + 5] = {};
                if (newrv == 0)
                {
                    cout << "timeout, newrv= " << newrv << endl;

                    break;
                }
                else if ((recved = recv(localSocket, ch, BUFF_SIZE, 0)) == -1)
                {
                    cerr << "recv failed, received bytes = " << recved << endl;
                }
                cout << ch << endl;
                for (int cnt = 0; cnt < 1024 && cnt + cnt_count < FILESIZE;)
                {
                    ff.write(&ch[cnt++], 1);
                    ff.flush();
                }
                cnt_count += 1024;
            }
            string command = "chmod 777 " + File_path;
            system(command.c_str());

            ff.close();
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
