'use strict';

const common = require('../common');
const http = require('http');
const assert = require('assert');
const { Writable } = require('stream');
const Countdown = require('../common/countdown');

const N = 2;
let abortRequest = true;

const server = http.Server(common.mustCall((req, res) => {
  const headers = { 'Content-Type': 'text/plain' };
  headers['Content-Length'] = 50;
  const socket = res.socket;
  res.writeHead(200, headers);
  res.write('aaaaaaaaaabbbbbbbbbbccccccccccdddddddddd');
  if (abortRequest) {
    process.nextTick(() => socket.destroy());
  } else {
    process.nextTick(() => res.end('eeeeeeeeee'));
  }
}, N));

server.listen(0, common.mustCall(() => {
  download();
}));

const finishCountdown = new Countdown(N, common.mustCall(() => {
  server.close();
}));
const reqCountdown = new Countdown(N, common.mustCall());

function download() {
  const opts = {
    port: server.address().port,
    path: '/',
  };
  const req = http.get(opts);
  req.on('error', common.mustNotCall());
  req.on('response', (res) => {
    assert.strictEqual(res.statusCode, 200);
    assert.strictEqual(res.headers.connection, 'close');
    let aborted = false;
    const writable = new Writable({
      write(chunk, encoding, callback) {
        callback();
      }
    });
    res.pipe(writable);
    const _handle = res.socket._handle;
    _handle._close = res.socket._handle.close;
    _handle.close = function(callback) {
      _handle._close();
      // Set readable to true even though request is complete
      if (res.complete) res.readable = true;
      callback();
    };
    res.on('end', common.mustCall(() => {
      reqCountdown.dec();
    }));
    res.on('aborted', () => {
      aborted = true;
    });
    res.on('error', common.mustNotCall());
    writable.on('finish', () => {
      assert.strictEqual(aborted, abortRequest);
      finishCountdown.dec();
      if (finishCountdown.remaining === 0) return;
      abortRequest = false; // next one should be a good response
      download();
    });
  });
  req.end();
}
