/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 * SPDX-License-Identifier: curl
 *
 ***************************************************************************/
/* <DESC>
 * Pass in a custom socket for libcurl to use.
 * </DESC>
 */

#include <systemd/sd-daemon.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <curl/curl.h>

#ifdef WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#define close closesocket
#else
#include <sys/types.h>        /*  socket types              */
#include <sys/socket.h>       /*  socket definitions        */
#include <netinet/in.h>
#include <arpa/inet.h>        /*  inet (3) functions         */
#include <unistd.h>           /*  misc. Unix functions      */
#endif

#include <errno.h>

/* The IP address and port number to connect to */
#define IPADDR "127.0.0.1"
#define PORTNUM 80

#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
  size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
  return written;
}

static int closecb(void *clientp, curl_socket_t item)
{
  (void)clientp;
  return 0;
}

static curl_socket_t opensocket(void *clientp,
                                curlsocktype purpose,
                                struct curl_sockaddr *address)
{
  curl_socket_t sockfd;
  (void)purpose;
  (void)address;
  sockfd = *(curl_socket_t *)clientp;
  /* the actual externally set socket is passed in via the OPENSOCKETDATA
     option */
  return sockfd;
}

static int sockopt_callback(void *clientp, curl_socket_t curlfd,
                            curlsocktype purpose)
{
  (void)clientp;
  (void)curlfd;
  (void)purpose;
  /* This return code was added in libcurl 7.21.5 */
  return CURL_SOCKOPT_ALREADY_CONNECTED;
}

int main(int argc, char *argv[])
{
  CURL *curl;
  CURLcode res;
  struct sockaddr_in servaddr;  /*  socket address structure  */
  curl_socket_t sockfd;

  if (argc != 3) {
    fprintf(stderr, "error: incorrect number of arguments");
    exit(1);
  }
  /* don't allow the special file descriptor names listed in the
     sd_listen_fds man page */
  if ((strcmp(argv[1], "unknown") == 0) ||
      (strcmp(argv[1], "stored") == 0) ||
      (strcmp(argv[1], "connection") == 0)) {
    fprintf(stderr, "error: special file descriptor name given");
    exit(1);
  }
#ifdef WIN32
  WSADATA wsaData;
  int initwsa = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if(initwsa) {
    printf("WSAStartup failed: %d\n", initwsa);
    return 1;
  }
#endif

  curl = curl_easy_init();
  if(curl) {
    /*
     * Note that libcurl will internally think that you connect to the host
     * and port that you specify in the URL option.
     */

    curl_easy_setopt(curl, CURLOPT_URL, argv[2]);
    char **names = NULL;
    const int num_fds = sd_listen_fds_with_names(0, &names);

    int num_matches = 0;
    for (int i = 0; i < num_fds; ++i) {
      if (strcmp(argv[1], names[i]) == 0) {
        ++num_matches;
        sockfd = i + SD_LISTEN_FDS_START;
      }
    }
    if (num_matches != 1) {
      fprintf(stderr, "error: this demo expects to be run from a systemd service where a unix-socket is passed in via the systemd directive OpenFile=/path/to/socket:fdname\n");
      exit(1);
    }
    /* TODO: add a check that sockfd really is a AF_UNIX socket.
     */
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port   = htons(PORTNUM);

    servaddr.sin_addr.s_addr = inet_addr(IPADDR);
    if(INADDR_NONE == servaddr.sin_addr.s_addr) {
      close(sockfd);
      return 2;
    }


    /* no progress meter please */
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);

    /* send all data to this function  */
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);

    /* call this function to get a socket */
    curl_easy_setopt(curl, CURLOPT_OPENSOCKETFUNCTION, opensocket);
    curl_easy_setopt(curl, CURLOPT_OPENSOCKETDATA, &sockfd);

    /* call this function to close sockets */
    curl_easy_setopt(curl, CURLOPT_CLOSESOCKETFUNCTION, closecb);
    curl_easy_setopt(curl, CURLOPT_CLOSESOCKETDATA, &sockfd);

    /* call this function to set options for the socket */
    curl_easy_setopt(curl, CURLOPT_SOCKOPTFUNCTION, sockopt_callback);

    curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);

    res = curl_easy_perform(curl);

    curl_easy_cleanup(curl);

    close(sockfd);

    if(res) {
      printf("libcurl error: %d\n", res);
      return 4;
    }
  }

#ifdef WIN32
  WSACleanup();
#endif
  return 0;
}
