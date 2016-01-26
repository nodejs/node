'use strict';

module.exports = { isLegalPort };

// Check that the port number is not NaN when coerced to a number,
// is an integer and that it falls within the legal range of port numbers.
function isLegalPort(port) {
  if (typeof port === 'string' && port.trim() === '')
    return false;
  return +port === (port >>> 0) && port >= 0 && port <= 0xFFFF;
}
