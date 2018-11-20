'use strict';

/*
 * This test is a regression test for joyent/node#8897.
 */

const common = require('../common');
const net = require('net');
const Countdown = require('../common/countdown');

const clients = [];

const server = net.createServer(function onClient(client) {
  clients.push(client);

  if (clients.length === 2) {
    /*
     * Enroll two timers, and make the one supposed to fire first
     * unenroll the other one supposed to fire later. This mutates
     * the list of unref timers when traversing it, and exposes the
     * original issue in joyent/node#8897.
     */
    clients[0].setTimeout(1, () => {
      clients[1].setTimeout(0);
      clients[0].end();
      clients[1].end();
    });

    // Use a delay that is higher than the lowest timer resolution across all
    // supported platforms, so that the two timers don't fire at the same time.
    clients[1].setTimeout(50);
  }
});

server.listen(0, common.localhostIPv4, common.mustCall(() => {
  const countdown = new Countdown(2, () => server.close());

  {
    const client = net.connect({ port: server.address().port });
    client.on('end', () => countdown.dec());
  }

  {
    const client = net.connect({ port: server.address().port });
    client.on('end', () => countdown.dec());
  }
}));
