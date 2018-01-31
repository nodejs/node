'use strict';

const Buffer = require('buffer').Buffer;
const { isIPv6 } = process.binding('cares_wrap');
const { writeBuffer } = process.binding('fs');

const octet = '(?:[0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])';
const re = new RegExp(`^${octet}[.]${octet}[.]${octet}[.]${octet}$`);

function isIPv4(s) {
  return re.test(s);
}

function isIP(s) {
  if (isIPv4(s)) return 4;
  if (isIPv6(s)) return 6;
  return 0;
}

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
  isIP,
  isIPv4,
  isIPv6,
  isLegalPort,
  makeSyncWrite,
  normalizedArgsSymbol: Symbol('normalizedArgs')
};
