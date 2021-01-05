#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
int n;
char recvbuf[1024];
void listen_board();
static void dealSigAlarm(int sigo)
{
    n = -1;
    printf("alarm interrupt!\n");
    return; //just interrupt the recvfrom()
}
void main()
{
    struct sigaction alarmact;
     signal(SIGALRM,dealSigAlarm);
    bzero(&alarmact, sizeof(alarmact));
    alarmact.sa_handler = dealSigAlarm;
     alarmact.sa_flags = SA_RESTART;
    alarmact.sa_flags = SA_NOMASK;
    sigaction(SIGALRM, &alarmact, NULL);
    listen_board();
}
void listen_board()
{
    int sock;
    struct sockaddr_in fromaddr;
    int len = sizeof(struct sockaddr_in);
    bzero(&fromaddr, len);
    fromaddr.sin_family = AF_INET;
    fromaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    fromaddr.sin_port = htons(9000);
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        perror("socket create error.\n");
    }
    while (1)
    {
        alarm(5);
        n = recvfrom(sock, recvbuf, 1024, 0, (struct sockaddr *)&fromaddr, &len);
        if (n < 0)
        {
            if (errno == EINTR)
                printf("recvfrom timeout.\n");
            else
                printf("recvfrom error.\n");
        }
        else
            alarm(0);
    }
}
