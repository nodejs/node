'use strict'
/**
 * A set of utilities borrowed from Node.js' _http_common.js
 */

/**
 * Verifies that the given val is a valid HTTP token
 * per the rules defined in RFC 7230
 * See https://tools.ietf.org/html/rfc7230#section-3.2.6
 *
 * Allowed characters in an HTTP token:
 * ^_`a-z  94-122
 * A-Z     65-90
 * -       45
 * 0-9     48-57
 * !       33
 * #$%&'   35-39
 * *+      42-43
 * .       46
 * |       124
 * ~       126
 *
 * This implementation of checkIsHttpToken() loops over the string instead of
 * using a regular expression since the former is up to 180% faster with v8 4.9
 * depending on the string length (the shorter the string, the larger the
 * performance difference)
 *
 * Additionally, checkIsHttpToken() is currently designed to be inlinable by v8,
 * so take care when making changes to the implementation so that the source
 * code size does not exceed v8's default max_inlined_source_size setting.
 **/
/* istanbul ignore next */
function isValidTokenChar (ch) {
  if (ch >= 94 && ch <= 122) { return true }
  if (ch >= 65 && ch <= 90) { return true }
  if (ch === 45) { return true }
  if (ch >= 48 && ch <= 57) { return true }
  if (ch === 34 || ch === 40 || ch === 41 || ch === 44) { return false }
  if (ch >= 33 && ch <= 46) { return true }
  if (ch === 124 || ch === 126) { return true }
  return false
}
/* istanbul ignore next */
function checkIsHttpToken (val) {
  if (typeof val !== 'string' || val.length === 0) { return false }
  if (!isValidTokenChar(val.charCodeAt(0))) { return false }
  const len = val.length
  if (len > 1) {
    if (!isValidTokenChar(val.charCodeAt(1))) { return false }
    if (len > 2) {
      if (!isValidTokenChar(val.charCodeAt(2))) { return false }
      if (len > 3) {
        if (!isValidTokenChar(val.charCodeAt(3))) { return false }
        for (var i = 4; i < len; i++) {
          if (!isValidTokenChar(val.charCodeAt(i))) { return false }
        }
      }
    }
  }
  return true
}
exports.checkIsHttpToken = checkIsHttpToken

/**
 * True if val contains an invalid field-vchar
 *  field-value    = *( field-content / obs-fold )
 *  field-content  = field-vchar [ 1*( SP / HTAB ) field-vchar ]
 *  field-vchar    = VCHAR / obs-text
 *
 * checkInvalidHeaderChar() is currently designed to be inlinable by v8,
 * so take care when making changes to the implementation so that the source
 * code size does not exceed v8's default max_inlined_source_size setting.
 **/
/* istanbul ignore next */
function checkInvalidHeaderChar (val) {
  val += ''
  if (val.length < 1) { return false }
  var c = val.charCodeAt(0)
  if ((c <= 31 && c !== 9) || c > 255 || c === 127) { return true }
  if (val.length < 2) { return false }
  c = val.charCodeAt(1)
  if ((c <= 31 && c !== 9) || c > 255 || c === 127) { return true }
  if (val.length < 3) { return false }
  c = val.charCodeAt(2)
  if ((c <= 31 && c !== 9) || c > 255 || c === 127) { return true }
  for (var i = 3; i < val.length; ++i) {
    c = val.charCodeAt(i)
    if ((c <= 31 && c !== 9) || c > 255 || c === 127) { return true }
  }
  return false
}
exports.checkInvalidHeaderChar = checkInvalidHeaderChar
