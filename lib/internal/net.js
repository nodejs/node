'use strict';

module.exports = { isLegalPort, assertPort };

// Check that the port number is not NaN when coerced to a number,
// is an integer and that it falls within the legal range of port numbers.
function isLegalPort(port) {
  if ((typeof port !== 'number' && typeof port !== 'string') ||
      (typeof port === 'string' && port.trim().length === 0))
    return false;
  return +port === (+port >>> 0) && port <= 0xFFFF;
}


function assertPort(port) {
  if (typeof port !== 'undefined' && !isLegalPort(port))
    throw new RangeError('"port" argument must be >= 0 and < 65536');
}
