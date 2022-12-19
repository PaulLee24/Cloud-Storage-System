#include <iostream>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/select.h>
#include <fcntl.h>

using namespace std;

#define MAX_LINE 1024

int main(int argc, char* argv[])
{
    if (argc != 4) {
        exit(1);
    }

    int sockfd;
    struct sockaddr_in servaddr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    int flag;
    flag = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flag | O_NONBLOCK);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(atoi(argv[2]));

    inet_pton(AF_INET, argv[1], &servaddr.sin_addr);

    int n, error;

    if ((n = connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr))) < 0) {
        if (errno != EINPROGRESS) {
            return(-1);
        }
    }

    fd_set rset, wset;
    FD_ZERO(&rset);
    FD_SET(sockfd, &rset);
    wset = rset;

    int max_fd;
    max_fd = sockfd + 1;

    if (n > 0) {
        n = select(max_fd, &rset, &wset, NULL, NULL);
        if (FD_ISSET(sockfd, &rset) || FD_ISSET(sockfd, &wset)) {
            socklen_t len = sizeof(error);
            if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
                return(-1);
            }
        }
        else {
            return(-1);
        }
    }

    char readline[MAX_LINE+2];
    char sendline[MAX_LINE+2];
    char recvline[MAX_LINE+2];
    
    int stdin_fd = fileno(stdin);
    flag = fcntl(stdin_fd, F_GETFL, 0);
    fcntl(stdin_fd, F_SETFL, flag | O_NONBLOCK);

    FD_SET(stdin_fd, &rset);

    max_fd = max(sockfd, stdin_fd) + 1;

    cout << "Welcome to the dropbox-like server: " << argv[3] << endl;

    bool success = false;
    string username = "0 ";
    username = username + argv[3];
    write(sockfd, username.c_str(), username.size());

    vector<string> WriteList;
    FD_ZERO(&wset);
    bool write_waiting = false;

    bool continue_read = false;
    int already_read_byte = 0;

    while (1)
    {
        FD_SET(stdin_fd, &rset);
        FD_SET(sockfd, &rset);

        if (write_waiting) {
            FD_SET(sockfd, &wset);
        }
        
        select(max_fd, &rset, &wset, NULL, NULL);

        char* OUTpath;
        FILE* output;

        if (FD_ISSET(stdin_fd, &rset))
        {
            n = read(stdin_fd, readline, MAX_LINE);
            readline[n] = '\0';
            char* cmd = strtok(readline, " \n");
            if (strcmp(cmd, "/put") == 0) {
                char* INpath = strtok(NULL, " \n");
                string sINpath = INpath;
                FILE* input = fopen(INpath, "rb");
                fseek(input, 0, SEEK_END);
                long int flen = ftell(input);
                //cout << "flen: " << flen << endl;
                fseek(input, 0, SEEK_SET);
                char num_buf[50];
                int NumSize = sprintf(num_buf, "%ld", flen);

                sendline[0] = '1';
                sendline[1] = ' ';
                copy(INpath, INpath+sINpath.size(), sendline+2);
                sendline[2+sINpath.size()] = ' ';
                copy(num_buf, num_buf+NumSize, sendline+3+sINpath.size());
                //cout << NumSize + 3 + sINpath.size() << endl;
                sendline[NumSize + 3 + sINpath.size()] = ' ';
                n = write(sockfd, sendline, NumSize+4+sINpath.size());
                if (n < 0) {
                    if (errno != EWOULDBLOCK) {
                        cout << "write error" << endl;
                        exit(1);
                    }
                    write_waiting = true;
                    WriteList.push_back(sINpath);
                    continue;
                }
                /*for (int x = 0; x < NumSize + 4 + strlen(INpath); x++) {
                    cout << sendline[x];
                }
                cout << "***" << endl;*/
                //cout << INpath << endl;
                //cout << "strlen(INpath): " << strlen(INpath) << endl;

                cout << "[Upload] " << sINpath << " Start!" << endl;

                int bytes;
                double t = double(flen) / 20;
                //cout << "t: " << t << endl;
                long int current = 0;
                cout << "Progress : [";
                while ((bytes = fread(readline, sizeof(char), 1022, input)) > 0) {
                    //sendline[0] = '2';
                    //sendline[1] = ' ';
                    copy(readline, readline+bytes, sendline);
                    write(sockfd, sendline, bytes);
                    long int i = double(current) / t;
                    long int j = double(current+bytes) / t;
                    for (long int k = i; k < j; k++) {
                        cout << "#";
                    }
                    current += bytes;
                }
                cout << "]" << endl;
                cout << "[Upload] " << sINpath << " Finish!" << endl;
                fclose(input);
            }
            else if (strcmp(cmd, "/sleep") == 0) {
                int sec = atoi(strtok(NULL, " \n"));
                cout << "The client starts to sleep." << endl;
                for (int i = 1; i <= sec; i++) {
                    cout << "Sleep " << i << endl;
                    sleep(1);
                }
                cout << "Client wakes up." << endl;
            }
            else if (strcmp(cmd, "/exit") == 0) {
                close(sockfd);
                return 0;
            }
        }

        if (FD_ISSET(sockfd, &rset))
        {
            if (continue_read) {
                int read_remain = MAX_LINE - already_read_byte;
                n = read(sockfd, readline + already_read_byte, read_remain);
                n = n + already_read_byte;
                continue_read = false;
                already_read_byte = 0;
            }
            else {
                n = read(sockfd, readline, MAX_LINE);
            }            
            readline[n] = '\0';
            //cout << "readline: " << readline << endl;
            char* cmd = strtok(readline, " ");
            if (strcmp(cmd, "1") == 0) {
                int headerLen = 2;
                char* fname = strtok(NULL, " ");
                string sfname = fname;
                FILE* output = fopen(fname, "wb");
                char* aflen = strtok(NULL, " ");
                string saflen = aflen;
                int flen = atoi(aflen);
                headerLen = headerLen + sfname.size() + saflen.size() + 2;
                //cout << sfname << " flen: " << flen << endl;
                int curLen = 0;
                double t = double(flen) / 20;
                cout << "[Download] " << sfname << " Start!" << endl;
                cout << "Progress : [";

                if (headerLen < n) {
                    //cout << "***" << endl;
                    int tlen = n - headerLen;
                    if (tlen > flen) {
                        continue_read = true;
                        already_read_byte = tlen - flen;
                        int i = double(curLen) / t;
                        int j = double(curLen + flen) / t;
                        for (int k = i; k < j; k++) {
                            cout << "#";
                        }
                        curLen += flen;
                        char* text = new char[flen];
                        copy(readline + headerLen, readline + headerLen + flen, text);
                        fwrite(text, sizeof(char), flen, output);
                        delete[]text;
                        char* readtemp = new char[already_read_byte];
                        copy(readline + headerLen + flen, readline + n, readtemp);
                        copy(readtemp, readtemp + already_read_byte, readline);
                        delete[]readtemp;
                    }
                    else {
                        int i = double(curLen) / t;
                        int j = double(curLen + tlen) / t;
                        for (int k = i; k < j; k++) {
                            cout << "#";
                        }
                        curLen += tlen;
                        char* text = new char[tlen];
                        copy(readline + headerLen, readline + n, text);
                        fwrite(text, sizeof(char), tlen, output);
                        delete[]text;
                    }                    
                }

                while (curLen < flen) {
                    int res = flen - curLen;
                    //cout << "curLen: " << curLen << " res: " << res << endl;
                    if (res < 1024) {
                        n = read(sockfd, readline, res);
                    }
                    else {
                        n = read(sockfd, readline, MAX_LINE);
                    }
                    if (n < 0) {
                        if (errno != EWOULDBLOCK) {
                            cout << "read error" << endl;
                            exit(1);
                        }
                    }
                    else if (n == 0) {
                        cout << "server closed" << endl;
                        exit(1);
                    }
                    else {
                        /*cout << "readline(while): " << endl;
                        for (int x = 0; x < 20; x++) {
                            cout << readline + x;
                        }
                        cout << endl;*/
                        //cout << curLen << endl;
                        int i = double(curLen) / t;
                        int j = double(curLen + n) / t;
                        for (int k = i; k < j; k++) {
                            cout << "#";
                        }
                        curLen += n;
                        char* text = new char[n];
                        copy(readline, readline + n, text);
                        fwrite(text, sizeof(char), n, output);
                        delete[]text;
                    }
                }
                fclose(output);
                cout << "]" << endl;
                cout << "[Download] " << sfname << " Finish!" << endl;
            }
        }
        if (FD_ISSET(sockfd, &wset)) {
            bool wsuccess = true;
            for (int k = 0; k < WriteList.size(); k++) {
                const char* INpath = WriteList[k].c_str();
                string sINpath = WriteList[k];
                FILE* input = fopen(INpath, "rb");
                fseek(input, 0, SEEK_END);
                long int flen = ftell(input);
                //cout << "flen: " << flen << endl;
                fseek(input, 0, SEEK_SET);
                char num_buf[50];
                int NumSize = sprintf(num_buf, "%ld", flen);

                sendline[0] = '1';
                sendline[1] = ' ';
                copy(INpath, INpath + sINpath.size(), sendline + 2);
                sendline[2 + sINpath.size()] = ' ';
                copy(num_buf, num_buf + NumSize, sendline + 3 + sINpath.size());
                //cout << NumSize + 3 + sINpath.size() << endl;
                sendline[NumSize + 3 + sINpath.size()] = ' ';
                n = write(sockfd, sendline, NumSize + 4 + sINpath.size());
                if (n < 0) {
                    if (errno != EWOULDBLOCK) {
                        cout << "write error" << endl;
                        exit(1);
                    }
                    wsuccess = false;
                    break;
                }

                cout << "[Upload] " << sINpath << " Start!" << endl;
                
                int bytes;
                double t = double(flen) / 20;
                long int current = 0;
                cout << "Progress : [";
                while ((bytes = fread(readline, sizeof(char), 1022, input)) > 0) {
                    //sendline[0] = '2';
                    //sendline[1] = ' ';
                    copy(readline, readline + bytes, sendline);
                    write(sockfd, sendline, bytes);
                    long int i = double(current) / t;
                    long int j = double(current + bytes) / t;
                    for (long int k = i; k < j; k++) {
                        cout << "#";
                    }
                    current += bytes;
                }
                cout << "]" << endl;
                cout << "[Upload] " << sINpath << " Finish!" << endl;
                fclose(input);
            }
            if (wsuccess) {
                WriteList.clear();
                write_waiting = false;
            }            
        }

    }

    return 0;
}

    

