'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const Countdown = require('../common/countdown');
const assert = require('assert');
const http2 = require('http2');

const {
  NGHTTP2_ENHANCE_YOUR_CALM,
} = http2.constants;

// Regression test: header blocks retained by stalled streams should continue
// to count against maxSessionMemory after they have been handed to JS.
const maxSessionMemory = 1;
const totalRequests = 400;
const cookieCrumbs = 120;

let accepted = 0;
let rejected = 0;

const server = http2.createServer({ maxSessionMemory });
server.on('stream', (stream) => {
  accepted++;
  stream.on('error', () => {});
  stream.respond();
  stream.write('x');
});

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`, {
    settings: {
      initialWindowSize: 0,
    },
  });
  client.on('error', () => {});

  client.on('remoteSettings', common.mustCall(() => {
    let destroyed = false;
    const countdown = new Countdown(totalRequests, common.mustCall(() => {
      assert(rejected > 0);
      assert(accepted < totalRequests);
      server.close();
    }));

    for (let i = 0; i < totalRequests; i++) {
      const headers = [':path', '/'];
      for (let j = 0; j < cookieCrumbs; j++) {
        headers.push('cookie', 'a=1');
      }

      const req = client.request(headers);
      req.on('error', () => {});
      req.on('close', () => {
        if (req.rstCode === NGHTTP2_ENHANCE_YOUR_CALM) {
          rejected++;
          if (!destroyed) {
            destroyed = true;
            client.destroy();
          }
        }
        countdown.dec();
      });
      req.end();
    }
  }));
}));
