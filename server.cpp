#include <iostream>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <map>
#include <utility>
#include <vector>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/select.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

using namespace std;

#define MAX_CHAR 1024
#define LISTENQ 1000

void GetFileNames(char* dir, vector<string> &fnames)
{
    DIR* dpdf;
    struct dirent* epdf;

    dpdf = opendir(dir);
    if (dpdf != NULL) {
        while (epdf = readdir(dpdf)) {
            if (strcmp(epdf->d_name, ".") == 0 || strcmp(epdf->d_name, "..") == 0) {
                continue;
            }
            fnames.push_back(epdf->d_name);
        }
    }
    closedir(dpdf);
}

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        exit(1);
    }

    int i, maxi, max_fd, listen_fd, conn_fd, sockfd, n_ready;
    int client[FD_SETSIZE];       // fd
    vector<string> Username(FD_SETSIZE);
    vector<vector<string>> WriteList(FD_SETSIZE);

    int already_write[FD_SETSIZE] = {0};

    string output;
    char cmd_line[MAX_CHAR+2];
    char recvline[MAX_CHAR+2];
    char sendline[MAX_CHAR+2];
    char readline[MAX_CHAR+2];
    ssize_t n;
    fd_set rset, allset;
    fd_set wset, wallset;
    socklen_t clilen;
    struct sockaddr_in cliaddr, servaddr;

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    int flag;
    flag = fcntl(listen_fd, F_GETFL, 0);
    fcntl(listen_fd, F_SETFL, flag | O_NONBLOCK);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(atoi(argv[1]));

    bind(listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr));

    listen(listen_fd, LISTENQ);

    max_fd = listen_fd + 1;
    maxi = -1;
    for (i = 0; i < FD_SETSIZE; i++)
    {
        client[i] = -1;
    }
    FD_ZERO(&allset);
    FD_ZERO(&wallset);
    FD_SET(listen_fd, &allset);

    bool continue_read = false;
    int already_read_byte = 0;

    while (1)
    {
        rset = allset;
        wset = wallset;
        n_ready = select(max_fd, &rset, &wset, NULL, NULL);
        if (FD_ISSET(listen_fd, &rset))
        {
            clilen = sizeof(cliaddr);
            if ((conn_fd = accept(listen_fd, (struct sockaddr*)&cliaddr, &clilen)) < 0) {
                cout << "accept error" << endl;
            }
            else {
                cout << "conn_fd: " << conn_fd << endl;
                flag = fcntl(conn_fd, F_GETFL, 0);
                fcntl(conn_fd, F_SETFL, flag | O_NONBLOCK);
                for (i = 0; i < FD_SETSIZE; i++)
                {
                    if (client[i] < 0)
                    {
                        client[i] = conn_fd;
                        break;
                    }
                }

                if (i == FD_SETSIZE)
                {
                    cout << "Too many clients!" << endl;
                    exit(2);
                }
                FD_SET(conn_fd, &allset);

                if (conn_fd >= max_fd) {
                    max_fd = conn_fd + 1;
                }
                if (i > maxi) {
                    maxi = i;
                }
            }

            n_ready--;
            if (n_ready <= 0) {
                continue;
            }
        }

        for (i = 0; i <= maxi; i++)
        {
            if (client[i] < 0) {
                continue;
            }
            if (FD_ISSET(client[i], &rset)) {
                if (continue_read) {
                    int read_remain = MAX_CHAR - already_read_byte;
                    n = read(client[i], cmd_line + already_read_byte, read_remain);
                    continue_read = false;
                    already_read_byte = 0;
                }
                else {
                    n = read(client[i], cmd_line, MAX_CHAR);
                }
                //n = read(client[i], cmd_line, MAX_CHAR);
                cmd_line[n] = '\0';
                //cout << n << " cmd_line: " << endl;
                //cout << cmd_line << endl;
                if (n == 0) {
                    cout << "Closed client " << i << endl;
                    close(client[i]);
                    FD_CLR(client[i], &allset);
                    client[i] = -1;
                    WriteList[i].clear();
                }
                else {
                    char* cmd = strtok(cmd_line, " ");
                    //cout << "cmd: " << cmd << endl;
                    //cout << cmd_line << endl;
                    if (strcmp(cmd, "0") == 0) {
                        char *username = strtok(NULL, " ");
                        int isCreate = mkdir(username, S_IRUSR | S_IWUSR | S_IXUSR | S_IRWXG | S_IRWXO);
                        string sname = username;
                        Username[i] = sname;
                        if (isCreate == -1) {
                            GetFileNames(username, WriteList[i]);
                            FD_SET(client[i], &wallset);
                        }
                    }
                    else if (strcmp(cmd, "1") == 0) {
                        int headerLen = 2;
                        char *fname = strtok(NULL, " ");
                        string sfname = fname;
                        string OUTpath = "./";
                        OUTpath = OUTpath + Username[i] + "/" + fname;
                        FILE* output = fopen(OUTpath.c_str(), "wb");
                        for (int k = 0; k < FD_SETSIZE; k++) {
                            if ((Username[k] == Username[i]) && (k != i)) {
                                FD_SET(client[k], &wallset);
                                WriteList[k].push_back(sfname);
                            }
                        }
                        char* aflen = strtok(NULL, " ");
                        string saflen = aflen;
                        int flen = atoi(aflen);
                        headerLen = headerLen + sfname.size() + saflen.size() + 2;                        
                        //cout << "flen: " << flen << endl;
                        int curLen = 0;
                        
                        /*if (headerLen < n) {
                            cout << "***" << endl;
                            int tlen = n - headerLen;
                            curLen += tlen;
                            char* text = new char[tlen];
                            copy(cmd_line+headerLen, cmd_line+n, text);
                            fwrite(text, sizeof(char), tlen, output);
                            delete[]text;
                        }*/

                        if (headerLen < n) {
                            //cout << "***" << endl;
                            int tlen = n - headerLen;
                            if (tlen > flen) {
                                continue_read = true;
                                already_read_byte = tlen - flen;
                                curLen += flen;
                                char* text = new char[flen];
                                copy(cmd_line + headerLen, cmd_line + headerLen + flen, text);
                                fwrite(text, sizeof(char), flen, output);
                                delete[]text;
                                char* readtemp = new char[already_read_byte];
                                copy(cmd_line + headerLen + flen, cmd_line + n, readtemp);
                                copy(readtemp, readtemp + already_read_byte, cmd_line);
                                delete[]readtemp;
                            }
                            else {
                                curLen += tlen;
                                char* text = new char[tlen];
                                copy(cmd_line + headerLen, cmd_line + n, text);
                                fwrite(text, sizeof(char), tlen, output);
                                delete[]text;
                            }
                        }

                        while (curLen < flen) {
                            //cout << "curLen" << curLen << endl;
                            int res = flen - curLen;
                            if (res < 1024) {
                                n = read(client[i], recvline, res);
                            }
                            else {
                                n = read(client[i], recvline, MAX_CHAR);
                            }                            
                            if (n < 0) {
                                if (errno != EWOULDBLOCK) {
                                    cout << "read error" << endl;
                                    exit(1);
                                }
                            }
                            else if (n == 0) {
                                cout << "Closed client " << i << endl;
                                close(client[i]);
                                FD_CLR(client[i], &allset);
                                client[i] = -1;
                                WriteList[i].clear();
                                break;
                            }
                            else {
                                curLen += n;
                                char* text = new char[n];
                                copy(recvline, recvline+n, text);
                                fwrite(text, sizeof(char), n, output);
                                delete[]text;
                            }
                        }
                        fclose(output);
                        cout << "upload finished" << endl;
                    }
                }
                n_ready--;
                if (n_ready <= 0) {
                    break;
                }
            }

            if (FD_ISSET(client[i], &wset)) {
                bool wsuccess = true;
                for (int k = 0; k < WriteList[i].size(); k++) {
                    string INpath = "./";
                    INpath = INpath + Username[i] + "/" + WriteList[i][k];
                    FILE* input = fopen(INpath.c_str(), "rb");
                    fseek(input, 0, SEEK_END);
                    long int flen = ftell(input);
                    fseek(input, 0, SEEK_SET);
                    char num_buf[50];
                    int NumSize = sprintf(num_buf, "%ld", flen);

                    const char *In_fname = WriteList[i][k].c_str();

                    cout << "Send " << WriteList[i][k] << " to " << Username[i] << " " << i << endl;

                    if (already_write[i] == 0) {
                        sendline[0] = '1';
                        sendline[1] = ' ';
                        copy(In_fname, In_fname + strlen(In_fname), sendline + 2);
                        sendline[2 + strlen(In_fname)] = ' ';
                        copy(num_buf, num_buf + NumSize, sendline + 3 + strlen(In_fname));
                        sendline[NumSize + 3 + strlen(In_fname)] = ' ';
                        n = write(client[i], sendline, NumSize + 4 + strlen(In_fname));
                        if (n < 0) {
                            if (errno != EWOULDBLOCK) {
                                cout << "write error" << endl;
                                exit(1);
                            }
                            cout << "Fail" << endl;
                            wsuccess = false;
                            break;
                        }
                    }                    

                    //cout << "Success" << endl;

                    int bytes;

                    if (already_write[i] != 0) {
                        int byte_count = 0;
                        while (byte_count < already_write[i]) {
                            int left_byte = already_write[i] - byte_count;
                            if (left_byte > 1022) {
                                bytes = fread(readline, sizeof(char), 1022, input);
                                byte_count += bytes;
                            }
                            else {
                                bytes = fread(readline, sizeof(char), left_byte, input);
                                byte_count += bytes;
                            }
                        }
                    }

                    while ((bytes = fread(readline, sizeof(char), 1022, input)) > 0) {
                        //sendline[0] = '2';
                        //sendline[1] = ' ';
                        copy(readline, readline + bytes, sendline);
                        n = write(client[i], sendline, bytes);
                        if (n < 0) {
                            if (errno != EWOULDBLOCK) {
                                cout << "write error" << endl;
                                exit(1);
                            }
                            wsuccess = false;
                            cout << "write block" << endl;
                            break;
                        }
                        else if (n < bytes) {
                            wsuccess = false;
                            cout << "write undone" << endl;
                            cout << "send " << n << endl;
                            already_write[i] += n;
                            break;
                        }
                        else {
                            //cout << "Send" << endl;
                            already_write[i] += n;
                        }                        
                    }
                    if (!wsuccess) {
                        break;
                    }
                    already_write[i] = 0;
                    fclose(input);
                }

                if (wsuccess) {
                    FD_CLR(client[i], &wallset);
                    WriteList[i].clear();
                    already_write[i] = 0;
                    cout << "wsuccess" << endl;
                }

                n_ready--;
                if (n_ready <= 0) {
                    break;
                }
            }
        }
    }

    return 0;
}