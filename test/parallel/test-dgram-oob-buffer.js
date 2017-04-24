'use strict';
// Some operating systems report errors when an UDP message is sent to an
// unreachable host. This error can be reported by sendto() and even by
// recvfrom(). Node should not propagate this error to the user.

const common = require('../common');
const dgram = require('dgram');

const socket = dgram.createSocket('udp4');
const buf = Buffer.from([1, 2, 3, 4]);
const portGetter = dgram.createSocket('udp4')
  .bind(0, 'localhost', common.mustCall(() => {
    const address = portGetter.address();
    portGetter.close(common.mustCall(() => {
      function ok() {}
      socket.send(buf, 0, 0, address.port, address.address, ok);
      socket.send(buf, 0, 4, address.port, address.address, ok);
      socket.send(buf, 1, 3, address.port, address.address, ok);
      socket.send(buf, 3, 1, address.port, address.address, ok);
      // Since length of zero means nothing, don't error despite OOB.
      socket.send(buf, 4, 0, address.port, address.address, ok);

      socket.close();
    }));
  }));
