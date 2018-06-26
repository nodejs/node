'use strict';

const assert = require('assert');
const util = require('util');
const { Socket } = require('net');
const { JSStream } = process.binding('js_stream');
const uv = process.binding('uv');
const debug = util.debuglog('stream_wrap');
const { ERR_STREAM_WRAP } = require('internal/errors').codes;

const kCurrentWriteRequest = Symbol('kCurrentWriteRequest');
const kCurrentShutdownRequest = Symbol('kCurrentShutdownRequest');

function isClosing() { return this.owner.isClosing(); }
function onreadstart() { return this.owner.readStart(); }
function onreadstop() { return this.owner.readStop(); }
function onshutdown(req) { return this.owner.doShutdown(req); }
function onwrite(req, bufs) { return this.owner.doWrite(req, bufs); }

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
    // Inside of the following functions, `this` refers to the handle
    // and `this.owner` refers to this JSStreamWrap instance.
    handle.isClosing = isClosing;
    handle.onreadstart = onreadstart;
    handle.onreadstop = onreadstop;
    handle.onshutdown = onshutdown;
    handle.onwrite = onwrite;

    stream.pause();
    stream.on('error', (err) => this.emit('error', err));
    const ondata = (chunk) => {
      if (typeof chunk === 'string' ||
          stream._readableState.objectMode === true) {
        // Make sure that no further `data` events will happen.
        stream.pause();
        stream.removeListener('data', ondata);

        this.emit('error', new ERR_STREAM_WRAP());
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
    this[kCurrentWriteRequest] = null;
    this[kCurrentShutdownRequest] = null;
    this.readable = stream.readable;
    this.writable = stream.writable;

    // Start reading.
    this.read(0);
  }

  // Legacy
  static get StreamWrap() {
    return JSStreamWrap;
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
    assert.strictEqual(this[kCurrentShutdownRequest], null);
    this[kCurrentShutdownRequest] = req;

    // TODO(addaleax): It might be nice if we could get into a state where
    // DoShutdown() is not called on streams while a write is still pending.
    //
    // Currently, the only part of the code base where that happens is the
    // TLS implementation, which calls both DoWrite() and DoShutdown() on the
    // underlying network stream inside of its own DoShutdown() method.
    // Working around that on the native side is not quite trivial (yet?),
    // so for now that is supported here.

    if (this[kCurrentWriteRequest] !== null)
      return this.on('drain', () => this.doShutdown(req));
    assert.strictEqual(this[kCurrentWriteRequest], null);

    const handle = this._handle;

    setImmediate(() => {
      // Ensure that write is dispatched asynchronously.
      this.stream.end(() => {
        this.finishShutdown(handle, 0);
      });
    });
    return 0;
  }

  // handle === this._handle except when called from doClose().
  finishShutdown(handle, errCode) {
    // The shutdown request might already have been cancelled.
    if (this[kCurrentShutdownRequest] === null)
      return;
    const req = this[kCurrentShutdownRequest];
    this[kCurrentShutdownRequest] = null;
    handle.finishShutdown(req, errCode);
  }

  doWrite(req, bufs) {
    assert.strictEqual(this[kCurrentWriteRequest], null);
    assert.strictEqual(this[kCurrentShutdownRequest], null);

    const handle = this._handle;
    const self = this;

    let pending = bufs.length;

    this.stream.cork();
    for (var i = 0; i < bufs.length; ++i)
      this.stream.write(bufs[i], done);
    this.stream.uncork();

    // Only set the request here, because the `write()` calls could throw.
    this[kCurrentWriteRequest] = req;

    function done(err) {
      if (!err && --pending !== 0)
        return;

      // Ensure that this is called once in case of error
      pending = 0;

      let errCode = 0;
      if (err) {
        errCode = uv[`UV_${err.code}`] || uv.UV_EPIPE;
      }

      // Ensure that write was dispatched
      setImmediate(() => {
        self.finishWrite(handle, errCode);
      });
    }

    return 0;
  }

  // handle === this._handle except when called from doClose().
  finishWrite(handle, errCode) {
    // The write request might already have been cancelled.
    if (this[kCurrentWriteRequest] === null)
      return;
    const req = this[kCurrentWriteRequest];
    this[kCurrentWriteRequest] = null;

    handle.finishWrite(req, errCode);
  }

  doClose(cb) {
    const handle = this._handle;

    setImmediate(() => {
      // Should be already set by net.js
      assert.strictEqual(this._handle, null);

      this.finishWrite(handle, uv.UV_ECANCELED);
      this.finishShutdown(handle, uv.UV_ECANCELED);

      cb();
    });
  }
}

module.exports = JSStreamWrap;
