'use strict';

const assert = require('assert');
const util = require('util');
const Socket = require('net').Socket;
const JSStream = process.binding('js_stream').JSStream;
const Buffer = require('buffer').Buffer;
const uv = process.binding('uv');
const debug = util.debuglog('stream_wrap');

function StreamWrap(stream) {
  const handle = new JSStream();

  this.stream = stream;

  this._list = null;

  handle.close = (cb) => {
    debug('close');
    this.doClose(cb);
  };
  handle.isAlive = () => {
    return this.isAlive();
  };
  handle.isClosing = () => {
    return this.isClosing();
  };
  handle.onreadstart = () => {
    return this.readStart();
  };
  handle.onreadstop = () => {
    return this.readStop();
  };
  handle.onshutdown = (req) => {
    return this.doShutdown(req);
  };
  handle.onwrite = (req, bufs) => {
    return this.doWrite(req, bufs);
  };

  this.stream.pause();
  this.stream.on('error', (err) => {
    this.emit('error', err);
  });
  this.stream.on('data', (chunk) => {
    if (!(chunk instanceof Buffer)) {
      // Make sure that no further `data` events will happen
      this.pause();
      this.removeListener('data', ondata);

      this.emit('error', new Error('Stream has StringDecoder'));
      return;
    }
    debug('data', chunk.length);
    if (this._handle)
      this._handle.readBuffer(chunk);
  });
  this.stream.once('end', () => {
    debug('end');
    if (this._handle)
      this._handle.emitEOF();
  });

  Socket.call(this, {
    handle: handle
  });
}
util.inherits(StreamWrap, Socket);
module.exports = StreamWrap;

// require('_stream_wrap').StreamWrap
StreamWrap.StreamWrap = StreamWrap;

StreamWrap.prototype.isAlive = function isAlive() {
  return true;
};

StreamWrap.prototype.isClosing = function isClosing() {
  return !this.readable || !this.writable;
};

StreamWrap.prototype.readStart = function readStart() {
  this.stream.resume();
  return 0;
};

StreamWrap.prototype.readStop = function readStop() {
  this.stream.pause();
  return 0;
};

StreamWrap.prototype.doShutdown = function doShutdown(req) {
  const handle = this._handle;
  const item = this._enqueue('shutdown', req);

  this.stream.end(() => {
    // Ensure that write was dispatched
    setImmediate(() => {
      if (!this._dequeue(item))
        return;

      handle.finishShutdown(req, 0);
    });
  });
  return 0;
};

StreamWrap.prototype.doWrite = function doWrite(req, bufs) {
  const handle = this._handle;

  var pending = bufs.length;

  // Queue the request to be able to cancel it
  const item = this._enqueue('write', req);

  this.stream.cork();
  bufs.forEach((buf) => {
    this.stream.write(buf, done);
  });
  this.stream.uncork();

  const done = (err) => {
    if (!err && --pending !== 0)
      return;

    // Ensure that this is called once in case of error
    pending = 0;

    // Ensure that write was dispatched
    setImmediate(() => {
      // Do not invoke callback twice
      if (!this._dequeue(item))
        return;

      var errCode = 0;
      if (err) {
        if (err.code && uv['UV_' + err.code])
          errCode = uv['UV_' + err.code];
        else
          errCode = uv.UV_EPIPE;
      }

      handle.doAfterWrite(req);
      handle.finishWrite(req, errCode);
    });
  };

  return 0;
};

function QueueItem(type, req) {
  this.type = type;
  this.req = req;
  this.prev = this;
  this.next = this;
}

StreamWrap.prototype._enqueue = function enqueue(type, req) {
  const item = new QueueItem(type, req);
  if (this._list === null) {
    this._list = item;
    return item;
  }

  item.next = this._list.next;
  item.prev = this._list;
  item.next.prev = item;
  item.prev.next = item;

  return item;
};

StreamWrap.prototype._dequeue = function dequeue(item) {
  assert(item instanceof QueueItem);

  var next = item.next;
  var prev = item.prev;

  if (next === null && prev === null)
    return false;

  item.next = null;
  item.prev = null;

  if (next === item) {
    prev = null;
    next = null;
  } else {
    prev.next = next;
    next.prev = prev;
  }

  if (this._list === item)
    this._list = next;

  return true;
};

StreamWrap.prototype.doClose = function doClose(cb) {
  const handle = this._handle;

  setImmediate(() => {
    while (this._list !== null) {
      const item = this._list;
      const req = item.req;
      this._dequeue(item);

      const errCode = uv.UV_ECANCELED;
      if (item.type === 'write') {
        handle.doAfterWrite(req);
        handle.finishWrite(req, errCode);
      } else if (item.type === 'shutdown') {
        handle.finishShutdown(req, errCode);
      }
    }

    // Should be already set by net.js
    assert(this._handle === null);
    cb();
  });
};
