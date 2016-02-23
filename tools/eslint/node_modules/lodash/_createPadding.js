var repeat = require('./repeat'),
    stringSize = require('./_stringSize'),
    stringToArray = require('./_stringToArray'),
    toInteger = require('./toInteger');

/** Used to compose unicode character classes. */
var rsAstralRange = '\\ud800-\\udfff',
    rsComboMarksRange = '\\u0300-\\u036f\\ufe20-\\ufe23',
    rsComboSymbolsRange = '\\u20d0-\\u20f0',
    rsVarRange = '\\ufe0e\\ufe0f';

/** Used to compose unicode capture groups. */
var rsZWJ = '\\u200d';

/** Used to detect strings with [zero-width joiners or code points from the astral planes](http://eev.ee/blog/2015/09/12/dark-corners-of-unicode/). */
var reHasComplexSymbol = RegExp('[' + rsZWJ + rsAstralRange  + rsComboMarksRange + rsComboSymbolsRange + rsVarRange + ']');

/* Built-in method references for those with the same name as other `lodash` methods. */
var nativeCeil = Math.ceil;

/**
 * Creates the padding for `string` based on `length`. The `chars` string
 * is truncated if the number of characters exceeds `length`.
 *
 * @private
 * @param {string} string The string to create padding for.
 * @param {number} [length=0] The padding length.
 * @param {string} [chars=' '] The string used as padding.
 * @returns {string} Returns the padding for `string`.
 */
function createPadding(string, length, chars) {
  length = toInteger(length);

  var strLength = stringSize(string);
  if (!length || strLength >= length) {
    return '';
  }
  var padLength = length - strLength;
  chars = chars === undefined ? ' ' : (chars + '');

  var result = repeat(chars, nativeCeil(padLength / stringSize(chars)));
  return reHasComplexSymbol.test(chars)
    ? stringToArray(result).slice(0, padLength).join('')
    : result.slice(0, padLength);
}

module.exports = createPadding;
