'use strict';

const { Buffer } = require('buffer');
const { FSReqWrap, close, read } = process.binding('fs');

const kReadFileBufferLength = 8 * 1024;

function readFileAfterRead(err, bytesRead) {
  const context = this.context;

  if (err)
    return context.close(err);

  if (bytesRead === 0)
    return context.close();

  context.pos += bytesRead;

  if (context.size !== 0) {
    if (context.pos === context.size)
      context.close();
    else
      context.read();
  } else {
    // unknown size, just read until we don't get bytes.
    context.buffers.push(context.buffer.slice(0, bytesRead));
    context.read();
  }
}

function readFileAfterClose(err) {
  const context = this.context;
  const callback = context.callback;
  let buffer = null;

  if (context.err || err)
    return callback(context.err || err);

  try {
    if (context.size === 0)
      buffer = Buffer.concat(context.buffers, context.pos);
    else if (context.pos < context.size)
      buffer = context.buffer.slice(0, context.pos);
    else
      buffer = context.buffer;

    if (context.encoding)
      buffer = buffer.toString(context.encoding);
  } catch (err) {
    return callback(err);
  }

  callback(null, buffer);
}

class ReadFileContext {
  constructor(callback, encoding) {
    this.fd = undefined;
    this.isUserFd = undefined;
    this.size = undefined;
    this.callback = callback;
    this.buffers = null;
    this.buffer = null;
    this.pos = 0;
    this.encoding = encoding;
    this.err = null;
  }

  read() {
    let buffer;
    let offset;
    let length;

    if (this.size === 0) {
      buffer = this.buffer = Buffer.allocUnsafeSlow(kReadFileBufferLength);
      offset = 0;
      length = kReadFileBufferLength;
    } else {
      buffer = this.buffer;
      offset = this.pos;
      length = Math.min(kReadFileBufferLength, this.size - this.pos);
    }

    const req = new FSReqWrap();
    req.oncomplete = readFileAfterRead;
    req.context = this;

    read(this.fd, buffer, offset, length, -1, req);
  }

  close(err) {
    const req = new FSReqWrap();
    req.oncomplete = readFileAfterClose;
    req.context = this;
    this.err = err;

    if (this.isUserFd) {
      process.nextTick(function tick() {
        req.oncomplete(null);
      });
      return;
    }

    close(this.fd, req);
  }
}

module.exports = ReadFileContext;
