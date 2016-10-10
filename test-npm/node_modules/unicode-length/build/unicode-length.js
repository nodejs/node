var REGEX_SYMBOLS, punycode, stripAnsi;

punycode = require('punycode');

stripAnsi = require('strip-ansi');

REGEX_SYMBOLS = /([\0-\u02FF\u0370-\u1DBF\u1E00-\u20CF\u2100-\uD7FF\uDC00-\uFE1F\uFE30-\uFFFF]|[\uD800-\uDBFF][\uDC00-\uDFFF]|[\uD800-\uDBFF])([\u0300-\u036F\u1DC0-\u1DFF\u20D0-\u20FF\uFE20-\uFE2F]+)/g;

exports.get = function(input) {
  var stripped;
  if (input == null) {
    throw new Error('Missing input');
  }
  if (typeof input !== 'string') {
    throw new Error("Invalid input: " + input);
  }
  input = stripAnsi(input);
  stripped = input.replace(REGEX_SYMBOLS, function($0, symbol, combiningMarks) {
    return symbol;
  });
  return punycode.ucs2.decode(stripped).length;
};
