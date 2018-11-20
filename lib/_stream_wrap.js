'use strict';

const util = require('util');
const Socket = require('net').Socket;
const JSStream = process.binding('js_stream').JSStream;
const uv = process.binding('uv');

function StreamWrap(stream) {
  var handle = new JSStream();

  this.stream = stream;

  var self = this;
  handle.close = function(cb) {
    cb();
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
    return self.shutdown(req);
  };
  handle.onwrite = function(req, bufs) {
    return self.write(req, bufs);
  };

  this.stream.pause();
  this.stream.on('data', function(chunk) {
    self._handle.readBuffer(chunk);
  });
  this.stream.once('end', function() {
    self._handle.emitEOF();
  });
  this.stream.on('error', function(err) {
    self.emit('error', err);
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

StreamWrap.prototype.shutdown = function shutdown(req) {
  var self = this;

  this.stream.end(function() {
    // Ensure that write was dispatched
    setImmediate(function() {
      self._handle.finishShutdown(req, 0);
    });
  });
  return 0;
};

StreamWrap.prototype.write = function write(req, bufs) {
  var pending = bufs.length;
  var self = this;

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
      var errCode = 0;
      if (err) {
        if (err.code && uv['UV_' + err.code])
          errCode = uv['UV_' + err.code];
        else
          errCode = uv.UV_EPIPE;
      }

      self._handle.doAfterWrite(req);
      self._handle.finishWrite(req, errCode);
    });
  }

  return 0;
};
