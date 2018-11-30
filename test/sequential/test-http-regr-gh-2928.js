// This test is designed to fail with a segmentation fault in Node.js 4.1.0 and
// execute without issues in Node.js 4.1.1 and up.

// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const httpCommon = require('_http_common');
const is_reused_symbol = require('internal/freelist').symbols.is_reused_symbol;
const { HTTPParser } = require('_http_common');
const net = require('net');

const COUNT = httpCommon.parsers.max + 1;

const parsers = new Array(COUNT);
for (let i = 0; i < parsers.length; i++)
  parsers[i] = httpCommon.parsers.alloc();

let gotRequests = 0;
let gotResponses = 0;

function execAndClose() {
  if (parsers.length === 0)
    return;
  process.stdout.write('.');

  const parser = parsers.pop();
  parser.reinitialize(HTTPParser.RESPONSE, parser[is_reused_symbol]);

  const socket = net.connect(common.PORT);
  socket.on('error', (e) => {
    // If SmartOS and ECONNREFUSED, then retry. See
    // https://github.com/nodejs/node/issues/2663.
    if (common.isSunOS && e.code === 'ECONNREFUSED') {
      parsers.push(parser);
      socket.destroy();
      setImmediate(execAndClose);
      return;
    }
    throw e;
  });

  parser.consume(socket._handle._externalStream);

  parser.onIncoming = function onIncoming() {
    process.stdout.write('+');
    gotResponses++;
    parser.unconsume(socket._handle._externalStream);
    httpCommon.freeParser(parser);
    socket.destroy();
    setImmediate(execAndClose);
  };
}

const server = net.createServer(function(c) {
  if (++gotRequests === COUNT)
    server.close();
  c.end('HTTP/1.1 200 OK\r\n\r\n', function() {
    c.destroySoon();
  });
}).listen(common.PORT, execAndClose);

process.on('exit', function() {
  assert.strictEqual(gotResponses, COUNT);
});
