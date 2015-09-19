'use strict';

var errnoException;

module.exports = function exceptionWithHostPort(err,
                                                syscall,
                                                address,
                                                port,
                                                additional) {
  var details;
  if (port && port > 0) {
    details = address + ':' + port;
  } else {
    details = address;
  }

  if (additional) {
    details += ' - Local (' + additional + ')';
  }
  if (errnoException === undefined)
    errnoException = require('internal/errno_exception');
  const ex = errnoException(err, syscall, details);
  ex.address = address;
  if (port) {
    ex.port = port;
  }
  return ex;
};
