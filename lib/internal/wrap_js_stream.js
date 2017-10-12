'use strict';

const assert = require('assert');
const util = require('util');
const { Socket } = require('net');
const { JSStream } = process.binding('js_stream');
// TODO(bmeurer): Change this back to const once hole checks are
// properly optimized away early in Ignition+TurboFan.
var Buffer = require('buffer').Buffer;
const uv = process.binding('uv');
const debug = util.debuglog('stream_wrap');

/* This class serves as a wrapper for when the C++ side of Node wants access
 * to a standard JS stream. For example, TLS or HTTP do not operate on network
 * resources conceptually, although that is the common case and what we are
 * optimizing for; in theory, they are completely composable and can work with
 * any stream resource they see.
 *
 * For the common case, i.e. a TLS socket wrapping around a net.Socket, we
 * can skip going through the JS layer and let TLS access the raw C++ handle
 * of a net.Socket. The flipside of this is that, to maintain composability,
 * we need a way to create "fake" net.Socket instances that call back into a
 * "real" JavaScript stream. JSStreamWrap is exactly this.
 */
class JSStreamWrap extends Socket {
  constructor(stream) {
    const handle = new JSStream();
    handle.close = (cb) => {
      debug('close');
      this.doClose(cb);
    };
    handle.isAlive = () => this.isAlive();
    handle.isClosing = () => this.isClosing();
    handle.onreadstart = () => this.readStart();
    handle.onreadstop = () => this.readStop();
    handle.onshutdown = (req) => this.doShutdown(req);
    handle.onwrite = (req, bufs) => this.doWrite(req, bufs);

    stream.pause();
    stream.on('error', (err) => this.emit('error', err));
    const ondata = (chunk) => {
      if (!(chunk instanceof Buffer)) {
        // Make sure that no further `data` events will happen.
        stream.pause();
        stream.removeListener('data', ondata);

        this.emit('error', new Error('Stream has StringDecoder'));
        return;
      }

      debug('data', chunk.length);
      if (this._handle)
        this._handle.readBuffer(chunk);
    };
    stream.on('data', ondata);
    stream.once('end', () => {
      debug('end');
      if (this._handle)
        this._handle.emitEOF();
    });

    super({ handle, manualStart: true });
    this.stream = stream;
    this._list = null;
    this.read(0);
  }

  // Legacy
  static get StreamWrap() {
    return JSStreamWrap;
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
  }

  doWrite(req, bufs) {
    const self = this;
    const handle = this._handle;

    var pending = bufs.length;

    // Queue the request to be able to cancel it
    const item = this._enqueue('write', req);

    this.stream.cork();
    for (var n = 0; n < bufs.length; n++)
      this.stream.write(bufs[n], done);
    this.stream.uncork();

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
  }

  doClose(cb) {
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
      assert.strictEqual(this._handle, null);
      cb();
    });
  }
}

function QueueItem(type, req) {
  this.type = type;
  this.req = req;
  this.prev = this;
  this.next = this;
}

module.exports = JSStreamWrap;
