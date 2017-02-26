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

  const self = this;
  handle.close = function(cb) {
    debug('close');
    self.doClose(cb);
  };
  handle.isAlive = function() {
    return self.isAlive();
  };
  handle.isClosing = function() {
    return self.isClosing();
  };
  handle.onreadstart = function() {
    return self.readStart();
  };
  handle.onreadstop = function() {
    return self.readStop();
  };
  handle.onshutdown = function(req) {
    return self.doShutdown(req);
  };
  handle.onwrite = function(req, bufs) {
    return self.doWrite(req, bufs);
  };

  this.stream.pause();
  this.stream.on('error', function onerror(err) {
    self.emit('error', err);
  });
  this.stream.on('data', function ondata(chunk) {
    if (!(chunk instanceof Buffer)) {
      // Make sure that no further `data` events will happen
      this.pause();
      this.removeListener('data', ondata);

      self.emit('error', new Error('Stream has StringDecoder'));
      return;
    }

    debug('data', chunk.length);
    if (self._handle)
      self._handle.readBuffer(chunk);
  });
  this.stream.once('end', function onend() {
    debug('end');
    if (self._handle)
      self._handle.emitEOF();
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
  const self = this;
  const handle = this._handle;
  const item = this._enqueue('shutdown', req);

  this.stream.end(function() {
    // Ensure that write was dispatched
    setImmediate(function() {
      if (!self._dequeue(item))
        return;

      handle.finishShutdown(req, 0);
    });
  });
  return 0;
};

StreamWrap.prototype.doWrite = function doWrite(req, bufs) {
  const self = this;
  const handle = self._handle;

  var pending = bufs.length;

  // Queue the request to be able to cancel it
  const item = self._enqueue('write', req);

  self.stream.cork();
  for (var n = 0; n < bufs.length; n++)
    self.stream.write(bufs[n], done);
  self.stream.uncork();

  function done(err) {
    if (!err && --pending !== 0)
      return;

    // Ensure that this is called once in case of error
    pending = 0;

    // Ensure that write was dispatched
    setImmediate(function() {
      // Do not invoke callback twice
      if (!self._dequeue(item))
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
  }

  return 0;
};

function QueueItem(type, req) {
  this.type = type;
  this.req = req;
  this.prev = this;
  this.next = this;
}

StreamWrap.prototype._enqueue = function _enqueue(type, req) {
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

StreamWrap.prototype._dequeue = function _dequeue(item) {
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
  const self = this;
  const handle = self._handle;

  setImmediate(function() {
    while (self._list !== null) {
      const item = self._list;
      const req = item.req;
      self._dequeue(item);

      const errCode = uv.UV_ECANCELED;
      if (item.type === 'write') {
        handle.doAfterWrite(req);
        handle.finishWrite(req, errCode);
      } else if (item.type === 'shutdown') {
        handle.finishShutdown(req, errCode);
      }
    }

    // Should be already set by net.js
    assert(self._handle === null);
    cb();
  });
};
