// Flags: --expose-internals
'use strict';

// Regression test: http.request({ highWaterMark }) must propagate the value
// to OutgoingMessage's kHighWaterMark so that _writeRaw() Path B (buffering
// before socket connects) uses the correct threshold.
//
// Without the fix:
//   - write() returns the wrong boolean (compares against default 64KB)
//   - On Node >= 24.16.0 (post #62936), this causes a deadlock when
//     the user awaits 'drain' after write() incorrectly returns false.
//
// Fixes: https://github.com/nodejs/node/issues/64645

const common = require('../common');
const assert = require('assert');
const http = require('http');
const { kHighWaterMark } = require('_http_outgoing');
const { getDefaultHighWaterMark } = require('internal/streams/state');

const server = http.createServer(common.mustCall((req, res) => {
  req.resume();
  req.on('end', () => res.end('ok'));
}, 3));

server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  let completed = 0;

  function done() {
    if (++completed === 3) server.close();
  }

  // Test 1: kHighWaterMark is set to user value on the ClientRequest.
  {
    const hwm = getDefaultHighWaterMark() * 2;
    const req = http.request({ port, method: 'POST', highWaterMark: hwm });
    assert.strictEqual(req[kHighWaterMark], hwm);
    req.end();
    req.on('response', (res) => { res.resume(); res.on('end', done); });
  }

  // Test 2: Large HWM — write below threshold returns true before socket connects.
  {
    const req = http.request({
      port,
      method: 'POST',
      highWaterMark: 100_000,
    }, common.mustCall((res) => {
      res.resume();
      res.on('end', done);
    }));

    // 64KB write in the same tick — socket not yet connected (Path B).
    // With HWM=100KB, write() must return true.
    const result = req.write(Buffer.alloc(64 * 1024));
    assert.strictEqual(result, true);
    req.end();
  }

  // Test 3: Small HWM — write above threshold returns false, drain fires.
  {
    const req = http.request({
      port,
      method: 'POST',
      highWaterMark: 512,
    }, common.mustCall((res) => {
      res.resume();
      res.on('end', done);
    }));

    // 2KB write in the same tick — exceeds HWM of 512 bytes.
    const result = req.write(Buffer.alloc(2 * 1024));
    assert.strictEqual(result, false);

    // Drain must fire (no deadlock) so we can complete the request.
    req.on('drain', common.mustCall(() => {
      req.end();
    }));
  }
}));
