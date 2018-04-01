'use strict';

const { Buffer } = require('buffer');
const errors = require('internal/errors');
const { WriteWrap } = process.binding('stream_wrap');

const errnoException = errors.errnoException;

function handleWriteReq(req, data, encoding) {
  const { handle } = req;

  switch (encoding) {
    case 'buffer':
      return handle.writeBuffer(req, data);
    case 'latin1':
    case 'binary':
      return handle.writeLatin1String(req, data);
    case 'utf8':
    case 'utf-8':
      return handle.writeUtf8String(req, data);
    case 'ascii':
      return handle.writeAsciiString(req, data);
    case 'ucs2':
    case 'ucs-2':
    case 'utf16le':
    case 'utf-16le':
      return handle.writeUcs2String(req, data);
    default:
      return handle.writeBuffer(req, Buffer.from(data, encoding));
  }
}

function createWriteWrap(handle, oncomplete) {
  var req = new WriteWrap();

  req.handle = handle;
  req.oncomplete = oncomplete;
  req.async = false;

  return req;
}

function writevGeneric(self, req, data, cb) {
  var allBuffers = data.allBuffers;
  var chunks;
  var i;
  if (allBuffers) {
    chunks = data;
    for (i = 0; i < data.length; i++)
      data[i] = data[i].chunk;
  } else {
    chunks = new Array(data.length << 1);
    for (i = 0; i < data.length; i++) {
      var entry = data[i];
      chunks[i * 2] = entry.chunk;
      chunks[i * 2 + 1] = entry.encoding;
    }
  }
  var err = req.handle.writev(req, chunks, allBuffers);

  // Retain chunks
  if (err === 0) req._chunks = chunks;

  afterWriteDispatched(self, req, err, cb);
}

function writeGeneric(self, req, data, encoding, cb) {
  var err = handleWriteReq(req, data, encoding);

  afterWriteDispatched(self, req, err, cb);
}

function afterWriteDispatched(self, req, err, cb) {
  if (err !== 0)
    return self.destroy(errnoException(err, 'write', req.error), cb);

  if (!req.async) {
    cb();
  } else {
    req.callback = cb;
  }
}

module.exports = {
  createWriteWrap,
  writevGeneric,
  writeGeneric
};
