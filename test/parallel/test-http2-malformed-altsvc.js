'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const http2 = require('http2');
const net = require('net');
const h2test = require('../common/http2');

const server = http2.createServer();
server.on('stream', common.mustNotCall());

const settings = new h2test.SettingsFrame();
const settingsAck = new h2test.SettingsFrame(true);
const altsvc = new h2test.AltSvcFrame((1 << 14) + 1);

server.listen(0, () => {
  const client = net.connect(server.address().port, () => {
    client.write(h2test.kClientMagic, () => {
      client.write(settings.data, () => {
        client.write(settingsAck.data);
        // Prior to nghttp2 1.31.1, sending this malformed altsvc frame
        // would cause a segfault. This test is successful if a segfault
        // does not occur.
        client.write(altsvc.data, common.mustCall(() => {
          client.destroy();
        }));
      });
    });
  });

  // An error may or may not be emitted on the client side, we don't care
  // either way if it is, but we don't want to die if it is.
  client.on('error', () => {});
  client.on('close', common.mustCall(() => server.close()));
  client.resume();
});
