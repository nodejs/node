'use strict';

const uv = process.binding('uv');

exports.printDeprecationMessage = function(msg, warned) {
  if (process.noDeprecation)
    return true;

  if (warned)
    return warned;

  if (process.throwDeprecation)
    throw new Error(msg);
  else if (process.traceDeprecation)
    console.trace(msg);
  else
    console.error(msg);

  return true;
};

// Mark that a method should not be used.
// Returns a modified function which warns once by default.
// If --no-deprecation is set, then it is a no-op.
exports.deprecate = function(fn, msg) {
  // Allow for deprecating things in the process of starting up.
  if (global.process === undefined) {
    return function() {
      return exports.deprecate(fn, msg).apply(this, arguments);
    };
  }

  if (process.noDeprecation === true) {
    return fn;
  }

  var warned = false;
  function deprecated() {
    warned = exports.printDeprecationMessage(msg, warned);
    return fn.apply(this, arguments);
  }

  return deprecated;
};

exports._errnoException = function(err, syscall, original) {
  const errname = uv.errname(err);
  var message = syscall + ' ' + errname;

  if (original)
    message += ' ' + original;

  const e = new Error(message);
  e.code = errname;
  e.errno = errname;
  e.syscall = syscall;

  return e;
};


exports._exceptionWithHostPort = function(err, syscall, address, port, extra) {
  var details;

  if (port && port > 0) {
    details = address + ':' + port;
  } else {
    details = address;
  }

  if (extra) {
    details += ' - Local (' + extra + ')';
  }

  const ex = exports._errnoException(err, syscall, details);

  ex.address = address;
  if (port) {
    ex.port = port;
  }

  return ex;
};
