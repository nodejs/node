'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const options = {
  key: fixtures.readKey('agent2-key.pem'),
  cert: fixtures.readKey('agent2-cert.pem')
};

const smallMessage = Buffer.from('hello world');
// Used to test .pause(), so needs to be larger than the internal buffer
const largeMessage = Buffer.alloc(64 * 1024).fill('hello world');

// Test typical usage
tls.createServer(options, common.mustCall(function(socket) {
  this.close();
  socket.end(smallMessage);
})).listen(0, function() {
  let received = 0;
  const buffers = [];
  const sockBuf = Buffer.alloc(8);
  tls.connect({
    port: this.address().port,
    rejectUnauthorized: false,
    onread: {
      buffer: sockBuf,
      callback: function(nread, buf) {
        assert.strictEqual(buf, sockBuf);
        received += nread;
        buffers.push(Buffer.from(buf.slice(0, nread)));
      }
    }
  }).on('data', common.mustNotCall()).on('end', common.mustCall(() => {
    assert.strictEqual(received, smallMessage.length);
    assert.deepStrictEqual(Buffer.concat(buffers), smallMessage);
  }));
});

// Test Uint8Array support
tls.createServer(options, common.mustCall(function(socket) {
  this.close();
  socket.end(smallMessage);
})).listen(0, function() {
  let received = 0;
  let incoming = new Uint8Array(0);
  const sockBuf = new Uint8Array(8);
  tls.connect({
    port: this.address().port,
    rejectUnauthorized: false,
    onread: {
      buffer: sockBuf,
      callback: function(nread, buf) {
        assert.strictEqual(buf, sockBuf);
        received += nread;
        const newIncoming = new Uint8Array(incoming.length + nread);
        newIncoming.set(incoming);
        newIncoming.set(buf.slice(0, nread), incoming.length);
        incoming = newIncoming;
      }
    }
  }).on('data', common.mustNotCall()).on('end', common.mustCall(() => {
    assert.strictEqual(received, smallMessage.length);
    assert.deepStrictEqual(incoming, new Uint8Array(smallMessage));
  }));
});

// Test Buffer callback usage
tls.createServer(options, common.mustCall(function(socket) {
  this.close();
  socket.end(smallMessage);
})).listen(0, function() {
  let received = 0;
  const incoming = [];
  const bufPool = [ Buffer.alloc(2), Buffer.alloc(2), Buffer.alloc(2) ];
  let bufPoolIdx = -1;
  let bufPoolUsage = 0;
  tls.connect({
    port: this.address().port,
    rejectUnauthorized: false,
    onread: {
      buffer: () => {
        ++bufPoolUsage;
        bufPoolIdx = (bufPoolIdx + 1) % bufPool.length;
        return bufPool[bufPoolIdx];
      },
      callback: function(nread, buf) {
        assert.strictEqual(buf, bufPool[bufPoolIdx]);
        received += nread;
        incoming.push(Buffer.from(buf.slice(0, nread)));
      }
    }
  }).on('data', common.mustNotCall()).on('end', common.mustCall(() => {
    assert.strictEqual(received, smallMessage.length);
    assert.deepStrictEqual(Buffer.concat(incoming), smallMessage);
    assert.strictEqual(bufPoolUsage, 7);
  }));
});

// Test Uint8Array callback support
tls.createServer(options, common.mustCall(function(socket) {
  this.close();
  socket.end(smallMessage);
})).listen(0, function() {
  let received = 0;
  let incoming = new Uint8Array(0);
  const bufPool = [ new Uint8Array(2), new Uint8Array(2), new Uint8Array(2) ];
  let bufPoolIdx = -1;
  let bufPoolUsage = 0;
  tls.connect({
    port: this.address().port,
    rejectUnauthorized: false,
    onread: {
      buffer: () => {
        ++bufPoolUsage;
        bufPoolIdx = (bufPoolIdx + 1) % bufPool.length;
        return bufPool[bufPoolIdx];
      },
      callback: function(nread, buf) {
        assert.strictEqual(buf, bufPool[bufPoolIdx]);
        received += nread;
        const newIncoming = new Uint8Array(incoming.length + nread);
        newIncoming.set(incoming);
        newIncoming.set(buf.slice(0, nread), incoming.length);
        incoming = newIncoming;
      }
    }
  }).on('data', common.mustNotCall()).on('end', common.mustCall(() => {
    assert.strictEqual(received, smallMessage.length);
    assert.deepStrictEqual(incoming, new Uint8Array(smallMessage));
    assert.strictEqual(bufPoolUsage, 7);
  }));
});

// Test explicit socket pause
tls.createServer(options, common.mustCall(function(socket) {
  this.close();
  // Need larger message here to observe the pause
  socket.end(largeMessage);
})).listen(0, function() {
  let received = 0;
  const buffers = [];
  const sockBuf = Buffer.alloc(64);
  let pauseScheduled = false;
  const client = tls.connect({
    port: this.address().port,
    rejectUnauthorized: false,
    onread: {
      buffer: sockBuf,
      callback: function(nread, buf) {
        assert.strictEqual(buf, sockBuf);
        received += nread;
        buffers.push(Buffer.from(buf.slice(0, nread)));
        if (!pauseScheduled) {
          pauseScheduled = true;
          client.pause();
          setTimeout(() => {
            client.resume();
          }, 100);
        }
      }
    }
  }).on('data', common.mustNotCall()).on('end', common.mustCall(() => {
    assert.strictEqual(received, largeMessage.length);
    assert.deepStrictEqual(Buffer.concat(buffers), largeMessage);
  }));
});

// Test implicit socket pause
tls.createServer(options, common.mustCall(function(socket) {
  this.close();
  // Need larger message here to observe the pause
  socket.end(largeMessage);
})).listen(0, function() {
  let received = 0;
  const buffers = [];
  const sockBuf = Buffer.alloc(64);
  let pauseScheduled = false;
  const client = tls.connect({
    port: this.address().port,
    rejectUnauthorized: false,
    onread: {
      buffer: sockBuf,
      callback: function(nread, buf) {
        assert.strictEqual(buf, sockBuf);
        received += nread;
        buffers.push(Buffer.from(buf.slice(0, nread)));
        if (!pauseScheduled) {
          pauseScheduled = true;
          setTimeout(() => {
            client.resume();
          }, 100);
          return false;
        }
        return true;
      }
    }
  }).on('data', common.mustNotCall()).on('end', common.mustCall(() => {
    assert.strictEqual(received, largeMessage.length);
    assert.deepStrictEqual(Buffer.concat(buffers), largeMessage);
  }));
});
