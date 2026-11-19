'use strict';

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const assert = require('node:assert');
const http2 = require('node:http2');
const { duplexPair } = require('node:stream');

// The callback passed to additionalHeaders() is called once the HEADERS frame
// has been written to the socket.
{
  const server = http2.createServer();

  server.on('stream', common.mustCall((stream) => {
    const { socket } = stream.session;
    const bytesWritten = socket.bytesWritten;
    let sync = true;

    for (const callback of [null, 1, 'callback', {}]) {
      assert.throws(() => stream.additionalHeaders({ ':status': 103 }, callback),
                    { code: 'ERR_INVALID_ARG_TYPE' });
    }

    // Informational headers without a callback do not shift the callbacks of
    // the ones that follow.
    stream.additionalHeaders({ ':status': 102 });
    stream.additionalHeaders({ ':status': 103 }, common.mustCall((err) => {
      assert.strictEqual(err, null);
      assert.strictEqual(sync, false);
      assert.ok(socket.bytesWritten > bytesWritten);
      stream.respond();
      stream.end('done');
    }));
    sync = false;
  }));

  server.listen(0, common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    const req = client.request();

    const seen = [];
    req.on('headers', (headers) => seen.push(headers[':status']));
    req.on('response', common.mustCall((headers) => {
      assert.strictEqual(headers[':status'], 200);
      assert.deepStrictEqual(seen, [102, 103]);
    }));

    req.resume();
    req.on('end', common.mustCall(() => {
      client.close();
      server.close();
    }));
  }));
}

// The callback is called with an error if the session is destroyed before the
// HEADERS frame is written.
{
  const server = http2.createServer();

  server.on('stream', common.mustCall((stream) => {
    stream.additionalHeaders({ ':status': 103 }, common.mustCall((err) => {
      assert.strictEqual(err.code, 'ECANCELED');
      server.close();
    }));
    stream.session.destroy();
  }));

  server.listen(0, common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    client.on('error', () => {});
    const req = client.request();
    req.on('headers', common.mustNotCall());
    req.on('error', () => {});
    req.on('close', common.mustCall(() => client.destroy()));
    req.resume();
  }));
}

// The callback is called with an error if the write to the underlying stream
// that carries the HEADERS frame fails.
{
  const kHeadersFrameType = 1;
  const hasHeadersFrame = (buf) => {
    for (let offset = 0; offset + 9 <= buf.length;) {
      if (buf[offset + 3] === kHeadersFrameType)
        return true;
      offset += 9 + buf.readUIntBE(offset, 3);
    }
    return false;
  };

  const [clientSide, serverSide] = duplexPair();
  const originalWrite = serverSide._write;
  serverSide._write = common.mustCallAtLeast(function(buf, enc, cb) {
    if (hasHeadersFrame(buf))
      return cb(new Error('write failed'));
    originalWrite.call(this, buf, enc, cb);
  });

  const server = http2.createServer();
  server.on('sessionError', common.mustCall());
  server.on('stream', common.mustCall((stream) => {
    stream.on('error', common.mustCall());
    stream.additionalHeaders({ ':status': 103 }, common.mustCall((err) => {
      assert.strictEqual(err.code, 'EPIPE');
      assert.strictEqual(err.syscall, 'write');
      client.destroy();
    }));
  }));
  server.emit('connection', serverSide);

  const client = http2.connect('http://localhost', {
    createConnection: () => clientSide,
  });
  client.on('error', () => {});
  const req = client.request();
  req.on('headers', common.mustNotCall());
  req.on('error', () => {});
  req.resume();
}

// The callback is called with an error if the stream is destroyed before the
// HEADERS frame has been serialized. This happens when another write to the
// underlying stream is still in progress, so that the frame is still queued.
{
  const [clientSide, serverSide] = duplexPair();
  let hold = false;
  let pendingWrite;
  let onHold;
  const originalWrite = serverSide._write;
  serverSide._write = common.mustCallAtLeast(function(buf, enc, cb) {
    if (!hold)
      return originalWrite.call(this, buf, enc, cb);
    // Deliver the data, but do not complete the write yet.
    originalWrite.call(this, buf, enc, common.mustCall(() => {
      pendingWrite = cb;
      onHold();
    }));
  });

  const server = http2.createServer();
  server.on('stream', common.mustCall((stream) => {
    onHold = common.mustCall(() => {
      stream.additionalHeaders({ ':status': 103 }, common.mustCall((err) => {
        assert.strictEqual(err.code, 'ECANCELED');
        hold = false;
        pendingWrite();
      }));
      stream.destroy();
    });
    hold = true;
    stream.additionalHeaders({ ':status': 102 });
  }));
  server.emit('connection', serverSide);

  const client = http2.connect('http://localhost', {
    createConnection: () => clientSide,
  });
  client.on('error', () => {});
  const req = client.request();
  const seen = [];
  req.on('headers', (headers) => seen.push(headers[':status']));
  req.on('error', () => {});
  req.on('close', common.mustCall(() => {
    assert.deepStrictEqual(seen, [102]);
    client.destroy();
  }));
  req.resume();
}
