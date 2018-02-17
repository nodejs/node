'use strict';

const common = require('../common');
const http = require('http');
const assert = require('assert');
const fs = require('fs');
const Countdown = require('../common/countdown');
const { URL } = require('url');

function cleanup(fname) {
  try {
    if (fs.statSync(fname)) fs.unlinkSync(fname);
  } catch (err) {}
}

const N = 1;
const fname = '/dev/null';
const keepAlive = false;
const useRemote = process.env.SPURIOUS_ABORTED_REMOTE === 'on';
let url = new URL('http://www.pdf995.com/samples/pdf.pdf');
let server;

const keepAliveAgent = new http.Agent({ keepAlive: true });
const reqCountdown = new Countdown(N, common.mustCall());

function download() {
  const opts = {
    host: url.hostname,
    path: url.pathname,
    port: url.port
  };
  if (keepAlive) opts.agent = keepAliveAgent;
  const req = http.get(opts);
  req.on('error', common.mustNotCall());
  req.on('response', (res) => {
    assert.strictEqual(res.statusCode, 200);
    if (keepAlive) {
      assert.strictEqual(res.headers.connection, 'keep-alive');
    } else {
      assert.strictEqual(res.headers.connection, 'close');
    }
    let aborted = false;
    const fstream = fs.createWriteStream(fname);
    res.pipe(fstream);
    res.on('end', common.mustCall(() => {
      reqCountdown.dec();
    }));
    res.on('aborted', () => {
      aborted = true;
    });
    res.on('error', common.mustNotCall());
    fstream.on('finish', () => {
      assert.strictEqual(aborted, false);
      cleanup(fname);
      finishCountdown.dec();
      if (finishCountdown.remaining === 0) return;
      download();
    });
  });
  req.end();
}

const finishCountdown = new Countdown(N, common.mustCall(() => {
  if (!useRemote) server.close();
}));

if (useRemote) {
  cleanup(fname);
  download();
} else {
  server = http.Server(common.mustCall((req, res) => {
    const headers = { 'Content-Type': 'text/plain' };
    if (req.url === '/content-length') {
      headers['Content-Length'] = 50;
    }
    const socket = res.socket;
    res.writeHead(200, headers);
    setTimeout(() => res.write('aaaaaaaaaa'), 100);
    setTimeout(() => res.write('bbbbbbbbbb'), 200);
    setTimeout(() => res.write('cccccccccc'), 300);
    setTimeout(() => res.write('dddddddddd'), 400);
    setTimeout(() => {
      res.end('eeeeeeeeee');
      const endAfter = Math.floor(Math.random() * 400);
      setTimeout(() => socket.destroy(), endAfter);
    }, 600);
  }, N));
  server.listen(0, common.mustCall(() => {
    url = new URL(`http://127.0.0.1:${server.address().port}/content-length`);
    cleanup(fname);
    download();
  }));
}
