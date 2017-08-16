
/* Copyright 1998 by the Massachusetts Institute of Technology.
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting
 * documentation, and that the name of M.I.T. not be used in
 * advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 */

#include "ares_setup.h"

#include "ares.h"
#include "ares_private.h"

void ares__close_sockets(ares_channel channel, struct server_state *server)
{
  struct send_request *sendreq;

  /* Free all pending output buffers. */
  while (server->qhead)
    {
      /* Advance server->qhead; pull out query as we go. */
      sendreq = server->qhead;
      server->qhead = sendreq->next;
      if (sendreq->data_storage != NULL)
        ares_free(sendreq->data_storage);
      ares_free(sendreq);
    }
  server->qtail = NULL;

  /* Reset any existing input buffer. */
  if (server->tcp_buffer)
    ares_free(server->tcp_buffer);
  server->tcp_buffer = NULL;
  server->tcp_lenbuf_pos = 0;

  /* Reset brokenness */
  server->is_broken = 0;

  /* Close the TCP and UDP sockets. */
  if (server->tcp_socket != ARES_SOCKET_BAD)
    {
      SOCK_STATE_CALLBACK(channel, server->tcp_socket, 0, 0);
      sclose(server->tcp_socket);
      server->tcp_socket = ARES_SOCKET_BAD;
      server->tcp_connection_generation = ++channel->tcp_connection_generation;
    }
  if (server->udp_socket != ARES_SOCKET_BAD)
    {
      SOCK_STATE_CALLBACK(channel, server->udp_socket, 0, 0);
      sclose(server->udp_socket);
      server->udp_socket = ARES_SOCKET_BAD;
    }
}
