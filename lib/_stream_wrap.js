'use strict';

const assert = require('assert');
const util = require('util');
const Socket = require('net').Socket;
const JSStream = process.binding('js_stream').JSStream;
const uv = process.binding('uv');

function StreamWrap(stream) {
  const handle = new JSStream();

  this.stream = stream;

  this._queue = null;

  const self = this;
  handle.close = function(cb) {
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
  this.stream.on('error', function(err) {
    self.emit('error', err);
  });

  Socket.call(this, {
    handle: handle
  });

  this.stream.on('data', function(chunk) {
    if (self._handle)
      self._handle.readBuffer(chunk);
  });
  this.stream.once('end', function() {
    if (self._handle)
      self._handle.emitEOF();
  });
}
util.inherits(StreamWrap, Socket);
module.exports = StreamWrap;

// require('_stream_wrap').StreamWrap
StreamWrap.StreamWrap = StreamWrap;

StreamWrap.prototype.isAlive = function isAlive() {
  return this.readable && this.writable;
};

StreamWrap.prototype.isClosing = function isClosing() {
  return !this.isAlive();
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

  this.stream.end(function() {
    // Ensure that write was dispatched
    setImmediate(function() {
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
  self._enqueue(req);

  self.stream.cork();
  bufs.forEach(function(buf) {
    self.stream.write(buf, done);
  });
  self.stream.uncork();

  function done(err) {
    if (!err && --pending !== 0)
      return;

    // Ensure that this is called once in case of error
    pending = 0;

    // Ensure that write was dispatched
    setImmediate(function() {
      // Do not invoke callback twice
      if (!self._dequeue(req))
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

StreamWrap.prototype._enqueue = function enqueue(req) {
  if (this._queue === null) {
    this._queue = req;
    req._prev = req;
    req._next = req;
    return;
  }

  req._next = this._queue._next;
  req._prev = this._queue;
  req._next._prev = req;
  req._prev._next = req;
};

StreamWrap.prototype._dequeue = function dequeue(req) {
  var next = req._next;
  var prev = req._prev;

  if (next === null && prev === null)
    return false;

  req._next = null;
  req._prev = null;

  if (next === req) {
    prev = null;
    next = null;
  } else {
    prev._next = next;
    next._prev = prev;
  }

  if (this._queue === req)
    this._queue = next;

  return true;
};

StreamWrap.prototype.doClose = function doClose(cb) {
  const self = this;
  const handle = self._handle;

  setImmediate(function() {
    while (self._queue !== null) {
      const req = self._queue;
      self._dequeue(req);

      const errCode = uv.UV_ECANCELED;
      handle.doAfterWrite(req);
      handle.finishWrite(req, errCode);
    }

    // Should be already set by net.js
    assert(self._handle === null);
    cb();
  });
};
