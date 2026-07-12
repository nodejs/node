'use strict';

// respondWithFD's 'finish' event must reflect actual file-pipe completion
// (matching every other Writable), not the moment respondWithFD was called.
//
//  - Clean EOF: 'finish' fires, writableFinished=true.
//  - Aborted before pipe completes: 'finish' does NOT fire,
//    writableFinished stays false. ('aborted' is preserved per its
//    legacy criterion: writable was 'ending' at close, so it doesn't
//    fire either way.)

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const http2 = require('http2');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

function testCleanCompletion(next) {
  const smallPath = path.join(tmpdir.path, 'small.bin');
  const smallSize = 64;
  fs.writeFileSync(smallPath, Buffer.alloc(smallSize));

  const server = http2.createServer();
  server.on('stream', common.mustCall((stream) => {
    stream.on('finish', common.mustCall(() => {
      assert.strictEqual(stream.writableFinished, true);
    }));
    stream.on('error', common.mustNotCall());
    stream.on('close', common.mustCall(() => {
      assert.strictEqual(stream.writableFinished, true);
    }));
    const fd = fs.openSync(smallPath, 'r');
    stream.ownsFd = true;
    stream.respondWithFD(fd, { 'content-length': smallSize });
  }));

  // Compat 'finish' fires only on real completion too.
  server.on('request', common.mustCall((req, res) => {
    res.on('finish', common.mustCall());
  }));

  server.listen(0, common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    const req = client.request();
    req.resume();
    req.on('end', common.mustCall());
    req.on('close', common.mustCall(() => {
      client.close();
      server.close(next);
    }));
  }));
}

function testAbortBeforeCompletion() {
  const smallPath = path.join(tmpdir.path, 'small-abort.bin');
  const smallSize = 64;
  fs.writeFileSync(smallPath, Buffer.alloc(smallSize));

  const server = http2.createServer();
  server.on('stream', common.mustCall((stream) => {
    stream.on('finish', common.mustNotCall(
      "'finish' must not fire on aborted file pipe"));
    stream.on('aborted', common.mustNotCall(
      "'aborted' must not fire - preserving deprecated legacy behaviour"));
    stream.on('error', () => {});
    stream.on('close', common.mustCall(() => {
      // writableFinished must reflect actual pipe completion.
      assert.strictEqual(stream.writableFinished, false);
    }));
    const fd = fs.openSync(smallPath, 'r');
    stream.ownsFd = true;
    stream.respondWithFD(fd, { 'content-length': smallSize });
    // Synchronously interrupt the pipe before it can read EOF.
    stream.destroy();
  }));

  // Compat 'finish' must not fire on aborted pipe either.
  server.on('request', common.mustCall((req, res) => {
    res.on('finish', common.mustNotCall(
      "compat res 'finish' must not fire on aborted file pipe"));
    res.on('close', common.mustCall());
  }));

  server.listen(0, common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    const req = client.request();
    req.on('error', () => {});
    req.on('close', common.mustCall(() => {
      client.close();
      server.close();
    }));
  }));
}

testCleanCompletion(testAbortBeforeCompletion);
