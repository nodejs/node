'use strict';

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
