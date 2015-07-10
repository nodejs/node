'use strict';

const prefix = '(node) ';

// All the internal deprecations have to use this function only, as this will
// prepend the prefix to the actual message.
exports.deprecate = function(fn, msg) {
  return exports._deprecate(fn, `${prefix}${msg}`);
};

// All the internal deprecations have to use this function only, as this will
// prepend the prefix to the actual message.
exports.printDeprecationMessage = function(msg, warned) {
  return exports._printDeprecationMessage(`${prefix}${msg}`, warned);
};

exports._printDeprecationMessage = function(msg, warned) {
  if (process.noDeprecation)
    return true;

  if (warned)
    return warned;

  if (process.throwDeprecation)
    throw new Error(msg);
  else if (process.traceDeprecation)
    console.trace(msg.startsWith(prefix) ? msg.replace(prefix, '') : msg);
  else
    console.error(msg);

  return true;
};

// Mark that a method should not be used.
// Returns a modified function which warns once by default.
// If --no-deprecation is set, then it is a no-op.
exports._deprecate = function(fn, msg) {
  // Allow for deprecating things in the process of starting up.
  if (global.process === undefined) {
    return function() {
      return exports._deprecate(fn, msg).apply(this, arguments);
    };
  }

  if (process.noDeprecation === true) {
    return fn;
  }

  var warned = false;
  function deprecated() {
    warned = exports._printDeprecationMessage(msg, warned);
    return fn.apply(this, arguments);
  }

  return deprecated;
};
