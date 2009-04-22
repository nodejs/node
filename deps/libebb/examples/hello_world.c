#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include <ev.h>
#include "ebb.h"

#define MSG ("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 12\r\n\r\nhello world\n")
static int c = 0;

struct hello_connection {
  unsigned int responses_to_write;
};

void on_close(ebb_connection *connection)
{
  free(connection->data);
  free(connection);
}

static void continue_responding(ebb_connection *connection)
{
  int r;
  struct hello_connection *connection_data = connection->data;
  //printf("response complete \n");
  if(--connection_data->responses_to_write > 0) {
    /* write another response */
    r = ebb_connection_write(connection, MSG, sizeof MSG, continue_responding);
    assert(r);
  } else {
    ebb_connection_schedule_close(connection);
  }
}

static void request_complete(ebb_request *request)
{
  //printf("request complete \n");
  ebb_connection *connection = request->data;
  struct hello_connection *connection_data = connection->data;

  if(ebb_request_should_keep_alive(request))
    connection_data->responses_to_write++;
  else
    connection_data->responses_to_write = 1;

  ebb_connection_write(connection, MSG, sizeof MSG, continue_responding);
  free(request);
}

static ebb_request* new_request(ebb_connection *connection)
{
  //printf("request %d\n", ++c);
  ebb_request *request = malloc(sizeof(ebb_request));
  ebb_request_init(request);
  request->data = connection;
  request->on_complete = request_complete;
  return request;
}

ebb_connection* new_connection(ebb_server *server, struct sockaddr_in *addr)
{
  struct hello_connection *connection_data = malloc(sizeof(struct hello_connection));
  if(connection_data == NULL)
    return NULL;
  connection_data->responses_to_write = 0;

  ebb_connection *connection = malloc(sizeof(ebb_connection));
  if(connection == NULL) {
    free(connection_data);
    return NULL;
  }

  ebb_connection_init(connection);
  connection->data = connection_data;
  connection->new_request = new_request;
  connection->on_close = on_close;
  
  printf("connection: %d\n", c++);
  return connection;
}

int main(int argc, char **_) 
{
  struct ev_loop *loop = ev_default_loop(0);
  ebb_server server;

  ebb_server_init(&server, loop);
  if(argc > 1) {
    printf("using SSL\n");
    ebb_server_set_secure(&server, "ca-cert.pem", "ca-key.pem");
  }
  server.new_connection = new_connection;

  printf("hello_world listening on port 5000\n");
  ebb_server_listen_on_port(&server, 5000);
  ev_loop(loop, 0);

  return 0;
}
