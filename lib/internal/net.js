'use strict';

const Buffer = require('buffer').Buffer;
const { writeBuffer } = process.binding('fs');

// Check that the port number is not NaN when coerced to a number,
// is an integer and that it falls within the legal range of port numbers.
function isLegalPort(port) {
  if ((typeof port !== 'number' && typeof port !== 'string') ||
      (typeof port === 'string' && port.trim().length === 0))
    return false;
  return +port === (+port >>> 0) && port <= 0xFFFF;
}

function makeSyncWrite(fd) {
  return function(chunk, enc, cb) {
    if (enc !== 'buffer')
      chunk = Buffer.from(chunk, enc);

    this._bytesDispatched += chunk.length;

    try {
      writeBuffer(fd, chunk, 0, chunk.length, null);
    } catch (ex) {
      // Legacy: net writes have .code === .errno, whereas writeBuffer gives the
      // raw errno number in .errno.
      if (typeof ex.code === 'string')
        ex.errno = ex.code;
      return cb(ex);
    }
    cb();
  };
}

module.exports = {
  isLegalPort,
  makeSyncWrite,
  normalizedArgsSymbol: Symbol('normalizedArgs')
};
