'use strict';

const assert = require('assert');
const util = require('util');
const Socket = require('net').Socket;
const JSStream = process.binding('js_stream').JSStream;
const uv = process.binding('uv');
const debug = util.debuglog('stream_wrap');
const errors = require('internal/errors');

class StreamWrap extends Socket {
  constructor(stream) {
    const handle = new JSStream();

    this.stream = stream;

    this._list = null;

    const self = this;
    handle.close = cb => {
      debug('close');
      self.doClose(cb);
    };
    handle.isAlive = () => self.isAlive();
    handle.isClosing = () => self.isClosing();
    handle.onreadstart = () => self.readStart();
    handle.onreadstop = () => self.readStop();
    handle.onshutdown = req => self.doShutdown(req);
    handle.onwrite = (req, bufs) => self.doWrite(req, bufs);

    this.stream.pause();
    this.stream.on('error', function onerror(err) {
      self.emit('error', err);
    });
    this.stream.on('data', function ondata(chunk) {
      if (typeof chunk === 'string' || this._readableState.objectMode === true) {
        // Make sure that no further `data` events will happen
        this.pause();
        this.removeListener('data', ondata);

        self.emit('error', new errors.Error('ERR_STREAM_WRAP'));
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

    super({
      handle
    });
  }

  isAlive() {
    return true;
  }

  isClosing() {
    return !this.readable || !this.writable;
  }

  readStart() {
    this.stream.resume();
    return 0;
  }

  readStop() {
    this.stream.pause();
    return 0;
  }

  doShutdown(req) {
    const self = this;
    const handle = this._handle;
    const item = this._enqueue('shutdown', req);

    this.stream.end(() => {
      // Ensure that write was dispatched
      setImmediate(() => {
        if (!self._dequeue(item))
          return;

        handle.finishShutdown(req, 0);
      });
    });
    return 0;
  }

  doWrite(req, bufs) {
    const self = this;
    const handle = self._handle;

    let pending = bufs.length;

    // Queue the request to be able to cancel it
    const item = self._enqueue('write', req);

    self.stream.cork();
    for (let n = 0; n < bufs.length; n++)
      self.stream.write(bufs[n], done);
    self.stream.uncork();

    function done(err) {
      if (!err && --pending !== 0)
        return;

      // Ensure that this is called once in case of error
      pending = 0;

      // Ensure that write was dispatched
      setImmediate(() => {
        // Do not invoke callback twice
        if (!self._dequeue(item))
          return;

        let errCode = 0;
        if (err) {
          if (err.code && uv[`UV_${err.code}`])
            errCode = uv[`UV_${err.code}`];
          else
            errCode = uv.UV_EPIPE;
        }

        handle.doAfterWrite(req);
        handle.finishWrite(req, errCode);
      });
    }

    return 0;
  }

  _enqueue(type, req) {
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
  }

  _dequeue(item) {
    assert(item instanceof QueueItem);

    let next = item.next;
    let prev = item.prev;

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
  }

  doClose(cb) {
    const self = this;
    const handle = self._handle;

    setImmediate(() => {
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
  }
}

module.exports = StreamWrap;

// require('_stream_wrap').StreamWrap
StreamWrap.StreamWrap = StreamWrap;

function QueueItem(type, req) {
  this.type = type;
  this.req = req;
  this.prev = this;
  this.next = this;
}
