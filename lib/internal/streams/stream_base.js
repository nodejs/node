'use strict';

const { Buffer } = require('buffer');
const errors = require('internal/errors');

const errnoException = errors.errnoException;

const StreamBaseMethods = {
  createWriteWrap(req, streamId, handle, oncomplete) {
    if (streamId)
      req.stream = streamId;

    req.handle = handle;
    req.oncomplete = oncomplete;
    req.async = false;
  },

  writeGeneric(req, writev, data, encoding, cb) {
    const { handle, error } = req;
    let err;

    if (writev) {
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
      err = this._handle.writev(req, chunks, allBuffers);

      // Retain chunks
      if (err === 0) req._chunks = chunks;
    } else {
      err = this._handleWriteReq(req, handle, data, encoding);
    }

    if (err) {
      return this.destroy(errnoException(err, 'write', error), cb);
    }
  },

  _handleWriteReq(req, handle, data, encoding) {
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
};

module.exports = StreamBaseMethods;
