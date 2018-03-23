'use strict';

const { Buffer } = require('buffer');
const errors = require('internal/errors');

const errnoException = errors.errnoException;

const kCreateWriteWrap = Symbol('createWriteWrap');
const kWritevGeneric = Symbol('writevGeneric');
const kWriteGeneric = Symbol('writeGeneric');

function _handleWriteReq(req, handle, data, encoding) {
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

const StreamBase = {
  [kCreateWriteWrap](req, handle, oncomplete) {
    req.handle = handle;
    req.oncomplete = oncomplete;
    req.async = false;
  },

  [kWritevGeneric](req, data, cb) {
    const { error } = req;
    const allBuffers = data.allBuffers;
    let chunks;
    let i;
    if (allBuffers) {
      chunks = data;
      for (i = 0; i < data.length; i++)
        data[i] = data[i].chunk;
    } else {
      chunks = new Array(data.length << 1);
      for (i = 0; i < data.length; i++) {
        const entry = data[i];
        chunks[i * 2] = entry.chunk;
        chunks[i * 2 + 1] = entry.encoding;
      }
    }
    const err = this._handle.writev(req, chunks, allBuffers);

    // Retain chunks
    if (err === 0) req._chunks = chunks;

    if (err)
      return this.destroy(errnoException(err, 'write', error), cb);
  },

  [kWriteGeneric](req, data, encoding, cb) {
    const { handle, error } = req;

    const err = _handleWriteReq(req, handle, data, encoding);

    if (err)
      return this.destroy(errnoException(err, 'write', error), cb);
  },

  apply(obj) {
    [
      kCreateWriteWrap,
      kWritevGeneric,
      kWriteGeneric
    ].forEach(function(sym) {
      obj[sym] = StreamBase[sym];
    });
  }
};

module.exports = {
  kCreateWriteWrap,
  kWritevGeneric,
  kWriteGeneric,
  StreamBase
};
