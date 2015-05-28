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
