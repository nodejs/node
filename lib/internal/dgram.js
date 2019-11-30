'use strict';

const {
  Symbol,
} = primordials;

const { codes } = require('internal/errors');
const { UDP } = internalBinding('udp_wrap');
const { guessHandleType } = internalBinding('util');
const { isInt32 } = require('internal/validators');
const { UV_EINVAL } = internalBinding('uv');
const { ERR_INVALID_ARG_TYPE, ERR_SOCKET_BAD_TYPE } = codes;
const kStateSymbol = Symbol('state symbol');
let dns;  // Lazy load for startup performance.


function lookup4(lookup, address, callback) {
  return lookup(address || '127.0.0.1', 4, callback);
}


function lookup6(lookup, address, callback) {
  return lookup(address || '::1', 6, callback);
}

function newHandle(type, lookup) {
  if (lookup === undefined) {
    if (dns === undefined) {
      dns = require('dns');
    }

    lookup = dns.lookup;
  } else if (typeof lookup !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('lookup', 'Function', lookup);
  }

  if (type === 'udp4') {
    const handle = new UDP();

    handle.lookup = lookup4.bind(handle, lookup);
    return handle;
  }

  if (type === 'udp6') {
    const handle = new UDP();

    handle.lookup = lookup6.bind(handle, lookup);
    handle.bind = handle.bind6;
    handle.connect = handle.connect6;
    handle.send = handle.send6;
    return handle;
  }

  throw new ERR_SOCKET_BAD_TYPE();
}


function _createSocketHandle(address, port, addressType, fd, flags) {
  const handle = newHandle(addressType);
  let err;

  if (isInt32(fd) && fd > 0) {
    const type = guessHandleType(fd);
    if (type !== 'UDP') {
      err = UV_EINVAL;
    } else {
      err = handle.open(fd);
    }
  } else if (port || address) {
    err = handle.bind(address, port || 0, flags);
  }

  if (err) {
    handle.close();
    return err;
  }

  return handle;
}


module.exports = {
  kStateSymbol,
  _createSocketHandle,
  newHandle
};
