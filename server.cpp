#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <arpa/inet.h>
#include <cstring>
#include <poll.h>
#include <errno.h>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <netinet/tcp.h>
#include <cmath>
#include <sstream>
#include <iomanip>

// Laborator 7 - https://beej.us/guide/bgnet/html/#poll
void add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size)
{
  // If we don't have room, add more space in the pfds array
  if (*fd_count == *fd_size)
  {
    *fd_size *= 2; // Double it

    *pfds = (struct pollfd *)realloc(*pfds, sizeof(**pfds) * (*fd_size));
  }

  (*pfds)[*fd_count].fd = newfd;
  (*pfds)[*fd_count].events = POLLIN; // Check ready-to-read

  (*fd_count)++;
}

// Laborator 7 - https://beej.us/guide/bgnet/html/#poll
void del_from_pfds(struct pollfd pfds[], int i, int *fd_count)
{
  // Copy the one from the end over this one
  pfds[i] = pfds[*fd_count - 1];

  (*fd_count)--;
}

bool string_contains(std::string s1, std::string s2)
{
  if (s1.find(s2) != std::string::npos)
    return true;

  return false;
}

typedef struct Client
{
  int sockfd;
  uint16_t port;
  std::string id, ip;
  Client(int sockfd, std::string id, std::string ip, uint16_t port)
  {
    this->sockfd = sockfd;
    this->id = id;
    this->ip = ip;
    this->port = port;
  }
} client_t;

bool check_key(std::unordered_map<int, client_t> a, int key)
{
  if (a.find(key) == a.end())
    return false;
  return true;
}

typedef struct ListenerUdp
{
  int sockfd;
  ListenerUdp()
  {
    this->sockfd = 0;
  }

  ListenerUdp(int port)
  {
    struct sockaddr_in servaddr;
    this->sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (this->sockfd < 0)
    {
      perror("socket creation failed");
      exit(0);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    if (bind(sockfd, (const struct sockaddr *)&servaddr,
             sizeof(servaddr)) < 0)
    {
      perror("bind failed");
      exit(EXIT_FAILURE);
    }
  }
} listener_udp_t;

typedef struct Listener
{
  int sockfd;
  Listener()
  {
    this->sockfd = 0;
  }
  Listener(int port)
  {
    struct sockaddr_in servaddr;
    this->sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (this->sockfd < 0)
    {
      perror("listen socket creation failed\n");
      exit(0);
    }

    std::memset(&servaddr, 0, sizeof(struct sockaddr_in));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    if ((bind(this->sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
    {
      printf("listen socket bind failed\n");
      exit(0);
    }

    if ((listen(this->sockfd, 1)) != 0)
    {
      printf("Listen failed\n");
      exit(0);
    }

    int nagle = 1;
    setsockopt(this->sockfd, IPPROTO_TCP, TCP_NODELAY, &nagle, sizeof(nagle));
  }
} listener_t;

typedef struct Server
{
  listener_t listener;
  listener_udp_t listener_udp;
  struct pollfd *pfds;
  int fd_count;
  int fd_size;
  int udp_sockfd;
  std::unordered_map<int, client_t> clients;
  std::vector<std::string> connecte_ids;
  std::unordered_map<std::string, std::vector<int>> topics;

  Server(int port)
  {
    this->listener = listener_t(port);
    this->listener_udp = listener_udp_t(port);
    this->pfds = new struct pollfd[10];
    pfds[0].fd = listener.sockfd;
    pfds[0].events = POLLIN;
    pfds[1].fd = listener_udp.sockfd;
    pfds[1].events = POLLIN;
    pfds[2].fd = 0;
    pfds[2].events = POLLIN;

    this->fd_count = 3;
    this->fd_size = 10;
  }

  // Laborator 7 - https://beej.us/guide/bgnet/html/#poll
  void run()
  {
    int connfd = 0, len = 0;
    struct sockaddr_in cli, udp;
    while (true)
    {
      int poll_count = poll(pfds, fd_count, -1);

      if (poll_count == -1)
      {
        exit(1);
      }

      for (int i = 0; i < fd_count; i++)
      {
        if (pfds[i].revents & POLLIN)
        {
          if (pfds[i].fd == listener.sockfd)
          {
            len = sizeof(cli);
            connfd = accept(listener.sockfd, (struct sockaddr *)&cli, (socklen_t *)&len);
            if (connfd < 0)
            {
              exit(0);
            }

            int nagle = 1;
            setsockopt(connfd, IPPROTO_TCP, TCP_NODELAY, &nagle, sizeof(nagle));

            char *buff = new char[10];
            std::memset(buff, 0, 10);

            int n = recv(connfd, buff, 10, 0);

            if (n < 0)
            {
              exit(0);
            }
            std::string id(buff);
            if (std::find(connecte_ids.begin(), connecte_ids.end(), id) != connecte_ids.end())
            {
              std::cout << "Client " + id + " already connected.\n";
              std::string fail_msg = "failed";
              n = send(connfd, fail_msg.c_str(), fail_msg.size(), 0);
              close(connfd);
            }
            else
            {
              std::string ip(inet_ntoa(cli.sin_addr));
              std::cout << "New client " + id + " connected from " + ip + ":" << htons(cli.sin_port) << ".\n";
              std::string success_msg = "success";
              n = send(connfd, success_msg.c_str(), success_msg.size(), 0);
              clients.insert(std::make_pair(connfd, client_t(connfd, id, ip, cli.sin_port)));
              connecte_ids.push_back(id);
              add_to_pfds(&pfds, connfd, &fd_count, &fd_size);
            }
            free(buff);
          }
          else if (pfds[i].fd == 0)
          {
            std::string command;
            std::getline(std::cin, command);

            if (string_contains(command, "exit"))
            {
              close(listener.sockfd);
              close(listener_udp.sockfd);
              std::string exit_command = "exit";
              for (std::unordered_map<int, client_t>::iterator iter = clients.begin(); iter != clients.end(); ++iter)
              {

                int client_sockfd = iter->first;
                send(client_sockfd, exit_command.c_str(), exit_command.size(), 0);
                free(pfds);
                close(client_sockfd);
              }
              exit(0);
            }
          }
          else if (pfds[i].fd == listener_udp.sockfd)
          {
            char *buff = new char[1551];
            std::memset(buff, 0, 1551);
            std::memset(&udp, 0, sizeof(udp));
            socklen_t len = sizeof(udp);
            int n = recvfrom(pfds[i].fd, buff, 1551, 0, (struct sockaddr *)&udp, &len);

            char *topic = new char[50];
            std::memset(topic, 0, 50);
            std::memcpy(topic, buff, 50);

            char *data_type = new char[1];
            std::memset(data_type, 0, 1);
            std::memcpy(data_type, buff + 50, 1);

            char *content = new char[1500];
            std::memset(content, 0, 1500);
            std::memcpy(content, buff + 51, 1500);

            std::string message = std::string(inet_ntoa(udp.sin_addr)) + ":" + std::to_string(udp.sin_port) + " - " + std::string(topic) + " - ";
            int type = data_type[0];
            if (type == 0)
            {
              message.append("INT - ");
              int sign = content[0];
              int data;
              std::memcpy(&data, content + 1, sizeof(int));

              data = ntohl(data);

              if (sign == 1)
                data = -data;

              message.append(std::to_string(data));
            }
            else if (type == 1)
            {
              message.append("SHORT_REAL - ");

              unsigned short aux_data;
              std::memcpy(&aux_data, content, sizeof(unsigned short));
              aux_data = ntohs(aux_data);

              std::stringstream data;
              data.setf(std::ios::fixed);
              data.precision(2);
              data << float(aux_data) / 100.f;

              message.append(data.str());
            }
            else if (type == 2)
            {
              message.append("FLOAT - ");
              int sign = content[0];
              int first;
              std::memcpy(&first, content + 1, sizeof(int));
              first = ntohl(first);

              if (sign == 1)
                first = -first;

              unsigned char negative;
              std::memcpy(&negative, content + 1 + sizeof(int), 1);

              float pow = std::pow(10.f, float(negative));
              float data = float(first) / pow;
              message.append(std::to_string(data));
            }
            else if (type == 3)
            {
              message.append("STRING - ");
              message.append(std::string(content));
            }

            if (topics.find(topic) != topics.end())
            {
              auto clients = topics.at(std::string(topic));
              for (int client_sock_fd : clients)
              {
                int n = send(client_sock_fd, message.c_str(), message.size(), 0);
              }
            }
            free(buff);
            free(topic);
            free(data_type);
            free(content);
          }
          else
          {
            char *buff = new char[1024];
            std::memset(buff, 0, 1024);
            //Bug foarte ciudat in care apare evenimentul de POLLIN totusi bufferul este gol si serverul nu a incercat sa trimita nimic
            //Poate e de la masina mea virtuala nu imi dau seama exact deoarece apare doar uneori :/
            int n = recv(pfds[i].fd, buff, 1024, MSG_PEEK | MSG_DONTWAIT);

            if (strlen(buff) == 0)
              continue;

            n = recv(pfds[i].fd, buff, 1024, 0);
            if (n < 0)
            {
              exit(0);
            }
            std::string command(buff);
            if (string_contains(command, "exit"))
            {

              client_t client = clients.at(pfds[i].fd);
              std::cout << "Client " + client.id + " disconnected.\n";
              clients.erase(pfds[i].fd);
              connecte_ids.erase(std::find(connecte_ids.begin(), connecte_ids.end(), client.id));
              close(pfds[i].fd);
              del_from_pfds(pfds, i, &fd_count);
            }
            else if (string_contains(command, "unsubscribe"))
            {
              std::string topic = command.substr(command.find(' ') + 1);
              auto clients = topics.at(topic);
              clients.erase(std::find(clients.begin(), clients.end(), pfds[i].fd));
              topics[topic] = clients;
            }
            else if (string_contains(command, "subscribe"))
            {
              std::string topic = command.substr(command.find(' ') + 1);
              if (topics.find(topic) == topics.end())
              {
                std::vector<int> clients;
                clients.push_back(pfds[i].fd);
                topics.insert(std::make_pair(topic, clients));
              }
              else
              {
                auto clients = topics.at(topic);
                clients.push_back(pfds[i].fd);
                topics[topic] = clients;
              }
            }
            free(buff);
          }
        }
      }
    }
  }
} server_t;

int main(int argc, char const *argv[])
{
  setvbuf(stdout, NULL, _IONBF, BUFSIZ);
  const int port = atoi(argv[1]);
  server_t server(port);

  server.run();

  return 0;
}