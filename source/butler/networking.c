#include "networking.h"

#include "utils.h"

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct Connection
{

} Connection;

typedef struct addrinfo addrinfo;
typedef struct sockaddr sockaddr;
typedef struct sockaddr_storage sockaddr_storage;

typedef int SOCKET;

#define BYTE_MAX 511

/* CONNECTOIN BASED */
Connection *wait_for_connection(unsigned port)
{
  (void)port;
  fprintf(stderr, "TODO: Implement wait_for_connection");
  return NULL;
}

void print_error_message(const char *fn_name, const char *filename, int line,
                         const char *host, unsigned port, const char *message)
{
  fprintf(stderr, "Error with host %s on port %d in function %s file %s:%d!\n",
          host, port, fn_name, filename, line);
  fprintf(stderr, "%s\n", message);
}

#define ERR(fn_name, host, port) print_error_message(fn_name, __FILE__,\
                                 __LINE__, host, port, strerror(errno))

#define CUST_ERR(fn_name, host, port, message) print_error_message(fn_name,\
                                                                   __FILE__,\
                                                                   __LINE__,\
                                                                   host, port,\
                                                                   message)

const char *wait_for_data(Connection *this, int buffer_size)
{
  (void)this; (void)buffer_size;
  fprintf(stderr, "TODO: Implement wait_for_data");
  return NULL;
}

void send_data(Connection *this, Datagram data)
{
  (void)this; (void)data;
  fprintf(stderr, "TODO: Implement send_data");
}

/* CONECTIONLESS */
SOCKET create_datagram_socket(const char *host, unsigned port, addrinfo *hints,
                              addrinfo **res);

/**
 * @brief Blocks until the selected port recieves a datagram. On an error prints
 * a message to sderr.
 *
 * @param port the port that will recieve the datagram
 *
 * @return the datagram that was recieved. If there is an error will return a
 * datagram with data set to NULL and a length of 0.
 */
Datagram receive_datagram(unsigned port)
{
  // return variable
  Datagram ret;
  ret.data   = NULL;
  ret.length = 0;

  const char *host = NULL;
  addrinfo hints, *res;

  hints.ai_flags = AI_PASSIVE;
  SOCKET sock = create_datagram_socket(NULL, port, &hints, &res);

  if (bind(sock, res->ai_addr, res->ai_addrlen) == -1)
    ERR("receive_datagram", host, port);
  else
  {
    char buffer[BYTE_MAX + 2];
    memset(buffer, 0, sizeof(buffer));

    sockaddr_storage src_addr;
    socklen_t src_addr_len = sizeof(src_addr);

    ssize_t count = recvfrom(sock, buffer, sizeof(buffer), 0,
                             (sockaddr*)&src_addr, &src_addr_len);

    if (count == -1)
      ERR("receive_datagram", host, port);
    else if (count == (ssize_t)sizeof(buffer))
      fprintf(stderr, "Error! Client sent too much data; Attendant names"
                      " should less than %lu characters!",
                      sizeof(buffer));
    else
    {
      char *data = malloc(sizeof(char) * count);
      memcpy(data, buffer, count);

      ret.data = data;
      ret.length = count;
    }
  }

  freeaddrinfo(res);
  close(sock);

  return ret;
}


/**
 * @brief Sends the given data to the given host on the given port. Creates
 * and closes a new socket to send the packet. On error prints a message to
 * stderr.
 *
 * @param data The data that should be sent to host.
 * @param host The host to send data to. May be an IP address or a hostname
 * @param port the port that the data should be sent to.
 *
 * @return true if the data was successfully sent, otherwise false
 */
bool send_datagram(Datagram data, const char *host, unsigned port)
{
  if (data.length > BYTE_MAX)
  {
    fprintf(stderr, "Error! Can only send %d bytes of data!", BYTE_MAX);
    return false;
  }

  static char buffer[BYTE_MAX + 2];

  // initialize the buffer
  memset(buffer, 0, sizeof(buffer));
  memcpy(buffer, data.data, data.length);
  buffer[data.length + 1] = '\n';

  // boilerplate
  addrinfo hints, *res;
  SOCKET sock = create_datagram_socket(host, port, &hints, &res);
  if (sock != -1)
  {
    // send the data
    int err = sendto(sock, buffer, data.length + 2, 0, res->ai_addr,
                     res->ai_addrlen);
    if (err == -1)
    {
      ERR("send_datagram", host, port);
      return false;
    }
  }
  else
  {
    ERR("send_datagram", host, port);
    return false;
  }

  freeaddrinfo(res);
  close(sock);

  return true;
}


/**
 * @brief Factors out common code between {send,receive}_datagram. It is
 * responsible for reserving a socket on the system for use by the caller. On
 * failure prints an error message to sderr.
 *
 * @param remote_host Specifies the machine that the target should connect
 * with. If the caller wants to listen for datagrams then this should be NULL
 *
 * @param port Specifies the relevant port. If the caller wants to listen for
 * datagrams then this should be the port that should be listened on. If the
 * caller wnats to send datagrams then this should be the port that sends
 * data.
 *
 * @param hints This structure is passed to getaddrinfo. The only variable that
 * should be set is ai_flags; this filed may have additional flags added to it,
 * while every other variable may be overwritten. For details on available
 * flags, `man getaddrinfo`
 *
 * @param res This is passed to getaddrinfo, and the result after getaddrinfo
 * will be passed to socket. No data needs to be set in this stucture. For more
 * information, `man getaddrinfo`
 *
 * @return Upon success returns the file descriptor of the newly opened
 * socket. Upon failure returns -1 and prints an error message to sderr.
 */
SOCKET create_datagram_socket(const char *remote_host, unsigned port,
                         addrinfo *hints, addrinfo **res)
{
  // return variable
  SOCKET sock = -1;

  // perpare hints
  int given_hints = hints->ai_flags;
  memset(hints, 0, sizeof(*hints));
  hints->ai_family = AF_UNSPEC;
  hints->ai_socktype = SOCK_DGRAM;
  hints->ai_flags = given_hints | AI_ADDRCONFIG;

  char str_port[6];
  memset(str_port, 0, sizeof(str_port));
  utoa(port, str_port);

  // create teh socket
  int err = getaddrinfo(remote_host, str_port, hints, res);
  if (err != 0)
  {
    // getaddrinfo only sometimes uses errno; gai_strerror takes this into
    // account
    CUST_ERR("create_datagram_socket", remote_host, port, gai_strerror(err));
  }
  else
  {
    sock = socket((*res)->ai_family, (*res)->ai_socktype,
                  (*res)->ai_protocol);

    if (sock == -1)
    {
      int save_errno = errno;
      struct sockaddr_in this;
      socklen_t size = sizeof(struct sockaddr_in);
      getsockname(sock, (struct sockaddr *)&this, &size);
      errno = save_errno;
      ERR("udp_setup", remote_host, this.sin_port);
    }
  }

  return sock;
}
