'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

const message = Buffer.from('hello world');

// Test typical usage
net.createServer(common.mustCall(function(socket) {
  this.close();
  socket.end(message);
})).listen(0, function() {
  let received = 0;
  const buffers = [];
  const sockBuf = Buffer.alloc(8);
  net.connect({
    port: this.address().port,
    onread: {
      buffer: sockBuf,
      callback: function(nread, buf) {
        assert.strictEqual(buf, sockBuf);
        received += nread;
        buffers.push(Buffer.from(buf.slice(0, nread)));
      }
    }
  }).on('data', common.mustNotCall()).on('end', common.mustCall(() => {
    assert.strictEqual(received, message.length);
    assert.deepStrictEqual(Buffer.concat(buffers), message);
  }));
});

// Test Uint8Array support
net.createServer(common.mustCall(function(socket) {
  this.close();
  socket.end(message);
})).listen(0, function() {
  let received = 0;
  let incoming = new Uint8Array(0);
  const sockBuf = new Uint8Array(8);
  net.connect({
    port: this.address().port,
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
    assert.strictEqual(received, message.length);
    assert.deepStrictEqual(incoming, new Uint8Array(message));
  }));
});

// Test Buffer callback usage
net.createServer(common.mustCall(function(socket) {
  this.close();
  socket.end(message);
})).listen(0, function() {
  let received = 0;
  const incoming = [];
  const bufPool = [ Buffer.alloc(2), Buffer.alloc(2), Buffer.alloc(2) ];
  let bufPoolIdx = -1;
  let bufPoolUsage = 0;
  net.connect({
    port: this.address().port,
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
    assert.strictEqual(received, message.length);
    assert.deepStrictEqual(Buffer.concat(incoming), message);
    assert.strictEqual(bufPoolUsage, 7);
  }));
});

// Test Uint8Array callback support
net.createServer(common.mustCall(function(socket) {
  this.close();
  socket.end(message);
})).listen(0, function() {
  let received = 0;
  let incoming = new Uint8Array(0);
  const bufPool = [ new Uint8Array(2), new Uint8Array(2), new Uint8Array(2) ];
  let bufPoolIdx = -1;
  let bufPoolUsage = 0;
  net.connect({
    port: this.address().port,
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
    assert.strictEqual(received, message.length);
    assert.deepStrictEqual(incoming, new Uint8Array(message));
    assert.strictEqual(bufPoolUsage, 7);
  }));
});

// Test explicit socket pause
net.createServer(common.mustCall(function(socket) {
  this.close();
  socket.end(message);
})).listen(0, function() {
  let received = 0;
  const buffers = [];
  const sockBuf = Buffer.alloc(8);
  let paused = false;
  net.connect({
    port: this.address().port,
    onread: {
      buffer: sockBuf,
      callback: function(nread, buf) {
        assert.strictEqual(paused, false);
        assert.strictEqual(buf, sockBuf);
        received += nread;
        buffers.push(Buffer.from(buf.slice(0, nread)));
        paused = true;
        this.pause();
        setTimeout(() => {
          paused = false;
          this.resume();
        }, 100);
      }
    }
  }).on('data', common.mustNotCall()).on('end', common.mustCall(() => {
    assert.strictEqual(received, message.length);
    assert.deepStrictEqual(Buffer.concat(buffers), message);
  }));
});

// Test implicit socket pause
net.createServer(common.mustCall(function(socket) {
  this.close();
  socket.end(message);
})).listen(0, function() {
  let received = 0;
  const buffers = [];
  const sockBuf = Buffer.alloc(8);
  let paused = false;
  net.connect({
    port: this.address().port,
    onread: {
      buffer: sockBuf,
      callback: function(nread, buf) {
        assert.strictEqual(paused, false);
        assert.strictEqual(buf, sockBuf);
        received += nread;
        buffers.push(Buffer.from(buf.slice(0, nread)));
        paused = true;
        setTimeout(() => {
          paused = false;
          this.resume();
        }, 100);
        return false;
      }
    }
  }).on('data', common.mustNotCall()).on('end', common.mustCall(() => {
    assert.strictEqual(received, message.length);
    assert.deepStrictEqual(Buffer.concat(buffers), message);
  }));
});
