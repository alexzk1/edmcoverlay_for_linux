// Copyright [2016] [Pedro Vicente]
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef LIB_SOCKET_H
#define LIB_SOCKET_H

#if defined (_MSC_VER)
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h> //hostent
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <errno.h>
    #include <unistd.h>
    #include <syslog.h>
#endif
#include <iostream>
#include <cerrno>
#include <string>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <ctime>
#include <memory>
#include <array>

#include "runners.h"

//multi platform socket descriptor
#if _WIN32
    typedef SOCKET socketfd_t;
#else
    typedef int socketfd_t;
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////
//utils
/////////////////////////////////////////////////////////////////////////////////////////////////////

std::string str_extract(const std::string& str_in);
std::string prt_time();
int set_daemon(const char* str_dir);
void wait(int nbr_secs);

/////////////////////////////////////////////////////////////////////////////////////////////////////
//socket_t
/////////////////////////////////////////////////////////////////////////////////////////////////////

class socket_t
{
public:
    socket_t();
    socket_t(socketfd_t sockfd, sockaddr_in sock_addr);
    void close();
    int write_all(const void* buf, int size_buf);
    int read_all(void* buf, int size_buf);
    int hostname_to_ip(const char* host_name, std::array<char, 100>& ip);

public:
    socketfd_t m_sockfd; //socket descriptor
    sockaddr_in
    m_sockaddr_in; //client address (used to store return value of server accept())
};

/////////////////////////////////////////////////////////////////////////////////////////////////////
//tcp_server_t
/////////////////////////////////////////////////////////////////////////////////////////////////////

class tcp_server_t : public socket_t
{
public:
    tcp_server_t(const unsigned short server_port);
    socket_t accept();
    std::shared_ptr<socket_t> accept_autoclose(utility::runnerint_t is_interrupted_ptr);
    ~tcp_server_t();
};

/////////////////////////////////////////////////////////////////////////////////////////////////////
//tcp_client_t
/////////////////////////////////////////////////////////////////////////////////////////////////////

class tcp_client_t : public socket_t
{
public:
    tcp_client_t();
    ~tcp_client_t();
    int connect();

    tcp_client_t(const char* host_name, const unsigned short server_port);
    int connect(const char* host_name, const unsigned short server_port);

protected:
    std::string m_server_ip;
    unsigned short m_server_port{0};
};

#endif
