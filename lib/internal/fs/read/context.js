'use strict';

const {
  ArrayPrototypePush,
  FunctionPrototypeCall,
  MathMin,
} = primordials;

const {
  constants: {
    kReadFileBufferLength,
    kReadFileUnknownBufferLength,
  },
  createReadFileBufferTooSmallError,
  getReadFileBuffer,
  getReadFileBufferByteLengthName,
} = require('internal/fs/utils');

const { Buffer } = require('buffer');

const { FSReqCallback, close, read } = internalBinding('fs');

const {
  AbortError,
  aggregateTwoErrors,
} = require('internal/errors');

function readFileAfterRead(err, bytesRead) {
  const context = this.context;

  if (err)
    return context.close(err);

  if (context.checkOverflow) {
    context.checkOverflow = false;

    if (bytesRead !== 0) {
      return context.close(createReadFileBufferTooSmallError(
        context.bufferByteLengthName,
        context.buffer.byteLength,
      ));
    }

    return context.close();
  }

  context.pos += bytesRead;

  if (context.userBuffer) {
    if (context.pos === context.size || bytesRead === 0) {
      context.close();
    } else {
      context.read();
    }
    return;
  }

  if (context.pos === context.size || bytesRead === 0) {
    context.close();
  } else {
    if (context.size === 0) {
      // Unknown size, just read until we don't get bytes.
      const buffer = bytesRead === kReadFileUnknownBufferLength ?
        context.buffer : context.buffer.slice(0, bytesRead);
      ArrayPrototypePush(context.buffers, buffer);
    }
    context.read();
  }
}

function readFileAfterClose(err) {
  const context = this.context;
  const callback = context.callback;
  let buffer = null;

  if (context.err || err)
    return callback(aggregateTwoErrors(err, context.err));

  try {
    if (context.userBuffer) {
      buffer = context.buffer.subarray(0, context.pos);
    } else if (context.size === 0) {
      buffer = Buffer.concat(context.buffers, context.pos);
    } else if (context.pos < context.size) {
      buffer = context.buffer.slice(0, context.pos);
    } else {
      buffer = context.buffer;
    }

    if (context.encoding)
      buffer = buffer.toString(context.encoding);
  } catch (err) {
    return callback(err);
  }

  callback(null, buffer);
}

class ReadFileContext {
  constructor(callback, options) {
    this.fd = undefined;
    this.isUserFd = undefined;
    this.size = 0;
    this.callback = callback;
    this.buffers = null;
    this.buffer = null;
    this.pos = 0;
    this.encoding = options.encoding;
    this.options = options;
    this.err = null;
    this.signal = undefined;
    this.userBuffer = false;
    this.bufferByteLengthName = null;
    this.overflowBuffer = null;
    this.checkOverflow = false;
  }

  prepare() {
    const buffer = getReadFileBuffer(this.options, this.size);

    if (buffer !== undefined) {
      this.buffer = buffer;
      this.userBuffer = true;
      this.bufferByteLengthName = getReadFileBufferByteLengthName(this.options);
      return;
    }

    if (this.size === 0) {
      this.buffers = [];
    } else {
      this.buffer = Buffer.allocUnsafeSlow(this.size);
    }
  }

  read() {
    let buffer;
    let offset;
    let length;

    if (this.signal?.aborted) {
      return this.close(
        new AbortError(undefined, { cause: this.signal.reason }));
    }

    if (this.userBuffer) {
      if (this.size === 0) {
        buffer = this.buffer;
        offset = this.pos;
        length = this.buffer.byteLength - this.pos;

        if (length === 0) {
          buffer = this.overflowBuffer ??= Buffer.allocUnsafeSlow(1);
          offset = 0;
          length = 1;
          this.checkOverflow = true;
        }
      } else {
        buffer = this.buffer;
        offset = this.pos;
        length = MathMin(kReadFileBufferLength, this.size - this.pos);
      }
    } else if (this.size === 0) {
      buffer = Buffer.allocUnsafeSlow(kReadFileUnknownBufferLength);
      offset = 0;
      length = kReadFileUnknownBufferLength;
      this.buffer = buffer;
    } else {
      buffer = this.buffer;
      offset = this.pos;
      length = MathMin(kReadFileBufferLength, this.size - this.pos);
    }

    const req = new FSReqCallback();
    req.oncomplete = readFileAfterRead;
    req.context = this;

    read(this.fd, buffer, offset, length, -1, req);
  }

  close(err) {
    if (this.isUserFd) {
      process.nextTick(function tick(context) {
        FunctionPrototypeCall(readFileAfterClose, { context }, null);
      }, this);
      return;
    }

    const req = new FSReqCallback();
    req.oncomplete = readFileAfterClose;
    req.context = this;
    this.err = err;

    close(this.fd, req);
  }
}

module.exports = ReadFileContext;
