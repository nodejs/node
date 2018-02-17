'use strict';

const common = require('../common');
const http = require('http');
const assert = require('assert');
const fs = require('fs');
const Countdown = require('../common/countdown');

function cleanup(fname) {
  try {
    if (fs.statSync(fname)) fs.unlinkSync(fname);
  } catch (err) {}
}

const N = 2;
const fname = '/dev/null';
let abortRequest = true;

const server = http.Server(common.mustCall((req, res) => {
  const headers = { 'Content-Type': 'text/plain' };
  headers['Content-Length'] = 50;
  const socket = res.socket;
  res.writeHead(200, headers);
  setTimeout(() => res.write('aaaaaaaaaa'), 100);
  setTimeout(() => res.write('bbbbbbbbbb'), 200);
  setTimeout(() => res.write('cccccccccc'), 300);
  setTimeout(() => res.write('dddddddddd'), 400);
  if (abortRequest) {
    setTimeout(() => socket.destroy(), 600);
  } else {
    setTimeout(() => res.end('eeeeeeeeee'), 1000);
  }
}, N));

server.listen(0, common.mustCall(() => {
  cleanup(fname);
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
    const fstream = fs.createWriteStream(fname);
    res.pipe(fstream);
    const _handle = res.socket._handle;
    _handle._close = res.socket._handle.close;
    _handle.close = function(callback) {
      _handle._close();
      // set readable to true event though request is complete
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
    fstream.on('finish', () => {
      assert.strictEqual(aborted, abortRequest);
      cleanup(fname);
      finishCountdown.dec();
      if (finishCountdown.remaining === 0) return;
      abortRequest = false; // next one should be a good response
      download();
    });
  });
  req.end();
}
