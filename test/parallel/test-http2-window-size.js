'use strict';

// This test ensures that servers are able to send data independent of window
// size.
// TODO: This test makes large buffer allocations (128KiB) and should be tested
// on smaller / IoT platforms in case this poses problems for these targets.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

// Given a list of buffers and an initial window size, have a server write
// each buffer to the HTTP2 Writable stream, and let the client verify that
// all of the bytes were sent correctly
function run(buffers, initialWindowSize) {
  return new Promise((resolve, reject) => {
    const expectedBuffer = Buffer.concat(buffers);

    const server = h2.createServer();
    server.on('stream', (stream) => {
      let i = 0;
      const writeToStream = () => {
        const cont = () => {
          i++;
          if (i < buffers.length) {
            setImmediate(writeToStream);
          } else {
            stream.end();
          }
        };
        const drained = stream.write(buffers[i]);
        if (drained) {
          cont();
        } else {
          stream.once('drain', cont);
        }
      };
      writeToStream();
    });
    server.listen(0);

    server.on('listening', common.mustCall(function() {
      const port = this.address().port;

      const client =
        h2.connect({
          authority: 'localhost',
          protocol: 'http:',
          port
        }, {
          settings: {
            initialWindowSize
          }
        }).on('connect', common.mustCall(() => {
          const req = client.request({
            ':method': 'GET',
            ':path': '/'
          });
          const responses = [];
          req.on('data', (data) => {
            responses.push(data);
          });
          req.on('end', common.mustCall(() => {
            const actualBuffer = Buffer.concat(responses);
            assert.strictEqual(Buffer.compare(actualBuffer, expectedBuffer), 0);
            // shut down
            client.close();
            server.close(() => {
              resolve();
            });
          }));
          req.end();
        }));
    }));
  });
}

const bufferValueRange = [0, 1, 2, 3];
const buffersList = [
  bufferValueRange.map((a) => Buffer.alloc(1 << 4, a)),
  bufferValueRange.map((a) => Buffer.alloc((1 << 8) - 1, a)),
// Specifying too large of a value causes timeouts on some platforms
//  bufferValueRange.map((a) => Buffer.alloc(1 << 17, a))
];
const initialWindowSizeList = [
  1 << 4,
  (1 << 8) - 1,
  1 << 8,
  1 << 17,
  undefined // use default window size which is (1 << 16) - 1
];

// Call `run` on each element in the cartesian product of buffersList and
// initialWindowSizeList.
let p = Promise.resolve();
for (const buffers of buffersList) {
  for (const initialWindowSize of initialWindowSizeList) {
    p = p.then(() => run(buffers, initialWindowSize));
  }
}
p.then(common.mustCall(() => {}));
