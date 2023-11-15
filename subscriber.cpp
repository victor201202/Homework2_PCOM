#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <poll.h>
#include <netinet/tcp.h>
#include <cstring>

bool string_contains(std::string s1, std::string s2)
{
    if (s1.find(s2) != std::string::npos)
        return true;

    return false;
}

std::string getSubString(
    const std::string &strValue,
    const char &startChar,
    const char &endChar)
{
    std::string subString = "";
    std::size_t startPos = strValue.find(startChar);
    std::size_t endPos = strValue.find(endChar, startPos + 1);
    if (startPos != std::string::npos &&
        endPos != std::string::npos)
    {
        subString = strValue.substr(startPos + 1, endPos - startPos - 1);
    }
    return subString;
}

int main(int argc, char const *argv[])
{
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    int status, valread, client_fd;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};
    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[3]));
    if (inet_pton(AF_INET, argv[2], &serv_addr.sin_addr) <= 0)
    {
        printf(
            "\nInvalid address/ Address not supported \n");
        return -1;
    }

    if ((status = connect(client_fd, (struct sockaddr *)&serv_addr,
                          sizeof(serv_addr))) < 0)
    {
        printf("\nConnection Failed \n");
        return -1;
    }
    int nagle = 1;
    int ret = setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &nagle, sizeof(nagle));
    send(client_fd, argv[1], strlen(argv[1]), 0);
    valread = recv(client_fd, buffer, 1024, 0);
    std::string response(buffer);
    if (string_contains(response, "failed"))
    {
        close(client_fd);
        exit(0);
    }

    int fd_count = 2;
    struct pollfd *pfds = (struct pollfd *)malloc(sizeof(struct pollfd) * 2);
    pfds[0].fd = 0;
    pfds[0].events = POLLIN;

    pfds[1].fd = client_fd;
    pfds[1].events = POLLIN;

    while (true)
    {
        int poll_count = poll(pfds, fd_count, -1);

        if (poll_count == -1)
        {
            perror("poll");
            exit(1);
        }

        for (int i = 0; i < fd_count; i++)
        {
            if (pfds[i].revents & POLLIN)
            {
                if (pfds[i].fd == client_fd)
                {
                    char *buff = new char[1024];
                    std::memset(buff, 0, 1024);
                    int n = recv(pfds[i].fd, buff, 1024, 0);
                    if (n < 0)
                    {
                        exit(0);
                    }


                    std::string command(buff);
                    if(string_contains(command, "exit"))
                    {
                        free(pfds);
                        close(client_fd);
                        exit(0);
                    }
                    else
                    {
                        std::cout << command << std::endl;
                    }
                    free(buff);
                }
                else
                {
                    std::string command;
                    std::getline(std::cin, command);

                    if (string_contains(command, "unsubscribe"))
                    {
                        std::cout << "Unsubscribed from topic.\n";
                    }
                    else if (string_contains(command, "subscribe"))
                    {
                        command = "subscribe " + getSubString(command, ' ', ' ');
                        std::cout << "Subscribed to topic.\n";
                        int n = send(client_fd, command.c_str(), command.size(), 0);
                        
                    }
                    else if (string_contains(command, "exit"))
                    {
                        std::string exit_msg = "exit";
                        send(client_fd, exit_msg.c_str(), exit_msg.size(), 0);
                        free(pfds);
                        close(client_fd);
                        exit(0);
                    }
                }
            }
        }
    }
    return 0;
}