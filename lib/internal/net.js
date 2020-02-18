'use strict';

const {
  RegExp,
  Symbol,
} = primordials;

const Buffer = require('buffer').Buffer;
const { writeBuffer } = internalBinding('fs');
const errors = require('internal/errors');

// IPv4 Segment
const v4Seg = '(?:[0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])';
const v4Str = `(${v4Seg}[.]){3}${v4Seg}`;
const IPv4Reg = new RegExp(`^${v4Str}$`);

// IPv6 Segment
const v6Seg = '(?:[0-9a-fA-F]{1,4})';
const IPv6Reg = new RegExp('^(' +
  `(?:${v6Seg}:){7}(?:${v6Seg}|:)|` +
  `(?:${v6Seg}:){6}(?:${v4Str}|:${v6Seg}|:)|` +
  `(?:${v6Seg}:){5}(?::${v4Str}|(:${v6Seg}){1,2}|:)|` +
  `(?:${v6Seg}:){4}(?:(:${v6Seg}){0,1}:${v4Str}|(:${v6Seg}){1,3}|:)|` +
  `(?:${v6Seg}:){3}(?:(:${v6Seg}){0,2}:${v4Str}|(:${v6Seg}){1,4}|:)|` +
  `(?:${v6Seg}:){2}(?:(:${v6Seg}){0,3}:${v4Str}|(:${v6Seg}){1,5}|:)|` +
  `(?:${v6Seg}:){1}(?:(:${v6Seg}){0,4}:${v4Str}|(:${v6Seg}){1,6}|:)|` +
  `(?::((?::${v6Seg}){0,5}:${v4Str}|(?::${v6Seg}){1,7}|:))` +
')(%[0-9a-zA-Z]{1,})?$');

function isIPv4(s) {
  return IPv4Reg.test(s);
}

function isIPv6(s) {
  return IPv6Reg.test(s);
}

function isIP(s) {
  if (isIPv4(s)) return 4;
  if (isIPv6(s)) return 6;
  return 0;
}

function makeSyncWrite(fd) {
  return function(chunk, enc, cb) {
    if (enc !== 'buffer')
      chunk = Buffer.from(chunk, enc);

    this._handle.bytesWritten += chunk.length;

    const ctx = {};
    writeBuffer(fd, chunk, 0, chunk.length, null, undefined, ctx);
    if (ctx.errno !== undefined) {
      const ex = errors.uvException(ctx);
      ex.errno = ctx.errno;
      return cb(ex);
    }
    cb();
  };
}

module.exports = {
  isIP,
  isIPv4,
  isIPv6,
  makeSyncWrite,
  normalizedArgsSymbol: Symbol('normalizedArgs')
};
