'use strict';

const common = require('../common');
const assert = require('assert');
const events = require('events');
const { createServer, request } = require('http');

events.captureRejections = true;

{
  const server = createServer(common.mustCall((req, res) => {
    const _err = new Error('kaboom');
    res.on('drain', common.mustCall(async () => {
      throw _err;
    }));

    res.socket.on('error', common.mustCall((err) => {
      assert.strictEqual(err, _err);
      server.close();
    }));

    // Write until there is space in the buffer
    res.writeHead(200, { 'Connection': 'close' });
    while (res.write('hello'));
  }));

  server.listen(0, common.mustCall(() => {
    const req = request({
      method: 'GET',
      host: server.address().host,
      port: server.address().port
    });

    req.end();

    req.on('response', common.mustCall((res) => {
      res.on('aborted', common.mustCall());
      res.on('error', common.expectsError({
        code: 'ECONNRESET'
      }));
      res.resume();
    }));
  }));
}

{
  let _res;
  let shouldEnd = false;
  // Not using mustCall here, because it is OS-dependant.
  const server = createServer((req, res) => {
    // So that we cleanly stop
    _res = res;

    if (shouldEnd) {
      res.end();
    }
  });

  server.listen(0, common.mustCall(() => {
    const _err = new Error('kaboom');

    const req = request({
      method: 'POST',
      host: server.address().host,
      port: server.address().port
    });

    req.on('response', common.mustNotCall((res) => {
      // So that we cleanly stop
      res.resume();
      server.close();
    }));

    req.on('error', common.mustCall((err) => {
      server.close();
      // On some variants of Windows, this can happen before
      // the server has received the request.
      if (_res) {
        _res.end();
      } else {
        shouldEnd = true;
      }
      assert.strictEqual(err, _err);
    }));

    req.on('drain', common.mustCall(async () => {
      throw _err;
    }));

    // Write until there is space in the buffer
    while (req.write('hello'));
  }));
}
