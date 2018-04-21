/** Used to escape characters for inclusion in compiled regexes. */
var regexpEscapes = {
  '0': 'x30', '1': 'x31', '2': 'x32', '3': 'x33', '4': 'x34',
  '5': 'x35', '6': 'x36', '7': 'x37', '8': 'x38', '9': 'x39',
  'A': 'x41', 'B': 'x42', 'C': 'x43', 'D': 'x44', 'E': 'x45', 'F': 'x46',
  'a': 'x61', 'b': 'x62', 'c': 'x63', 'd': 'x64', 'e': 'x65', 'f': 'x66',
  'n': 'x6e', 'r': 'x72', 't': 'x74', 'u': 'x75', 'v': 'x76', 'x': 'x78'
};

/** Used to escape characters for inclusion in compiled string literals. */
var stringEscapes = {
  '\\': '\\',
  "'": "'",
  '\n': 'n',
  '\r': 'r',
  '\u2028': 'u2028',
  '\u2029': 'u2029'
};

/**
 * Used by `_.escapeRegExp` to escape characters for inclusion in compiled regexes.
 *
 * @private
 * @param {string} chr The matched character to escape.
 * @param {string} leadingChar The capture group for a leading character.
 * @param {string} whitespaceChar The capture group for a whitespace character.
 * @returns {string} Returns the escaped character.
 */
function escapeRegExpChar(chr, leadingChar, whitespaceChar) {
  if (leadingChar) {
    chr = regexpEscapes[chr];
  } else if (whitespaceChar) {
    chr = stringEscapes[chr];
  }
  return '\\' + chr;
}

module.exports = escapeRegExpChar;
