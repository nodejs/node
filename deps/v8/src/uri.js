// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

// This file relies on the fact that the following declaration has been made
// in runtime.js:
// var $Array = global.Array;

// -------------------------------------------------------------------

// This file contains support for URI manipulations written in
// JavaScript.


(function() {

  // -------------------------------------------------------------------
  // Define internal helper functions.

  function HexValueOf(code) {
    // 0-9
    if (code >= 48 && code <= 57) return code - 48;
    // A-F
    if (code >= 65 && code <= 70) return code - 55;
    // a-f
    if (code >= 97 && code <= 102) return code - 87;

    return -1;
  }

  // Does the char code correspond to an alpha-numeric char.
  function isAlphaNumeric(cc) {
    // a - z
    if (97 <= cc && cc <= 122) return true;
    // A - Z
    if (65 <= cc && cc <= 90) return true;
    // 0 - 9
    if (48 <= cc && cc <= 57) return true;

    return false;
  }

  //Lazily initialized.
  var hexCharCodeArray = 0;

  function URIAddEncodedOctetToBuffer(octet, result, index) {
    result[index++] = 37; // Char code of '%'.
    result[index++] = hexCharCodeArray[octet >> 4];
    result[index++] = hexCharCodeArray[octet & 0x0F];
    return index;
  }

  function URIEncodeOctets(octets, result, index) {
    if (hexCharCodeArray === 0) {
      hexCharCodeArray = [48, 49, 50, 51, 52, 53, 54, 55, 56, 57,
                          65, 66, 67, 68, 69, 70];
    }
    index = URIAddEncodedOctetToBuffer(octets[0], result, index);
    if (octets[1]) index = URIAddEncodedOctetToBuffer(octets[1], result, index);
    if (octets[2]) index = URIAddEncodedOctetToBuffer(octets[2], result, index);
    if (octets[3]) index = URIAddEncodedOctetToBuffer(octets[3], result, index);
    return index;
  }

  function URIEncodeSingle(cc, result, index) {
    var x = (cc >> 12) & 0xF;
    var y = (cc >> 6) & 63;
    var z = cc & 63;
    var octets = new $Array(3);
    if (cc <= 0x007F) {
      octets[0] = cc;
    } else if (cc <= 0x07FF) {
      octets[0] = y + 192;
      octets[1] = z + 128;
    } else {
      octets[0] = x + 224;
      octets[1] = y + 128;
      octets[2] = z + 128;
    }
    return URIEncodeOctets(octets, result, index);
  }

  function URIEncodePair(cc1 , cc2, result, index) {
    var u = ((cc1 >> 6) & 0xF) + 1;
    var w = (cc1 >> 2) & 0xF;
    var x = cc1 & 3;
    var y = (cc2 >> 6) & 0xF;
    var z = cc2 & 63;
    var octets = new $Array(4);
    octets[0] = (u >> 2) + 240;
    octets[1] = (((u & 3) << 4) | w) + 128;
    octets[2] = ((x << 4) | y) + 128;
    octets[3] = z + 128;
    return URIEncodeOctets(octets, result, index);
  }

  function URIHexCharsToCharCode(highChar, lowChar) {
    var highCode = HexValueOf(highChar);
    var lowCode = HexValueOf(lowChar);
    if (highCode == -1 || lowCode == -1) {
      throw new $URIError("URI malformed");
    }
    return (highCode << 4) | lowCode;
  }

  // Callers must ensure that |result| is a sufficiently long sequential
  // two-byte string!
  function URIDecodeOctets(octets, result, index) {
    var value;
    var o0 = octets[0];
    if (o0 < 0x80) {
      value = o0;
    } else if (o0 < 0xc2) {
      throw new $URIError("URI malformed");
    } else {
      var o1 = octets[1];
      if (o0 < 0xe0) {
        var a = o0 & 0x1f;
        if ((o1 < 0x80) || (o1 > 0xbf)) {
          throw new $URIError("URI malformed");
        }
        var b = o1 & 0x3f;
        value = (a << 6) + b;
        if (value < 0x80 || value > 0x7ff) {
          throw new $URIError("URI malformed");
        }
      } else {
        var o2 = octets[2];
        if (o0 < 0xf0) {
          var a = o0 & 0x0f;
          if ((o1 < 0x80) || (o1 > 0xbf)) {
            throw new $URIError("URI malformed");
          }
          var b = o1 & 0x3f;
          if ((o2 < 0x80) || (o2 > 0xbf)) {
            throw new $URIError("URI malformed");
          }
          var c = o2 & 0x3f;
          value = (a << 12) + (b << 6) + c;
          if ((value < 0x800) || (value > 0xffff)) {
            throw new $URIError("URI malformed");
          }
        } else {
          var o3 = octets[3];
          if (o0 < 0xf8) {
            var a = (o0 & 0x07);
            if ((o1 < 0x80) || (o1 > 0xbf)) {
              throw new $URIError("URI malformed");
            }
            var b = (o1 & 0x3f);
            if ((o2 < 0x80) || (o2 > 0xbf)) {
              throw new $URIError("URI malformed");
            }
            var c = (o2 & 0x3f);
            if ((o3 < 0x80) || (o3 > 0xbf)) {
              throw new $URIError("URI malformed");
            }
            var d = (o3 & 0x3f);
            value = (a << 18) + (b << 12) + (c << 6) + d;
            if ((value < 0x10000) || (value > 0x10ffff)) {
              throw new $URIError("URI malformed");
            }
          } else {
            throw new $URIError("URI malformed");
          }
        }
      }
    }
    if (0xD800 <= value && value <= 0xDFFF) {
      throw new $URIError("URI malformed");
    }
    if (value < 0x10000) {
      %_TwoByteSeqStringSetChar(index++, value, result);
    } else {
      %_TwoByteSeqStringSetChar(index++, (value >> 10) + 0xd7c0, result);
      %_TwoByteSeqStringSetChar(index++, (value & 0x3ff) + 0xdc00, result);
    }
    return index;
  }

  // ECMA-262, section 15.1.3
  function Encode(uri, unescape) {
    var uriLength = uri.length;
    var array = new InternalArray(uriLength);
    var index = 0;
    for (var k = 0; k < uriLength; k++) {
      var cc1 = uri.charCodeAt(k);
      if (unescape(cc1)) {
        array[index++] = cc1;
      } else {
        if (cc1 >= 0xDC00 && cc1 <= 0xDFFF) throw new $URIError("URI malformed");
        if (cc1 < 0xD800 || cc1 > 0xDBFF) {
          index = URIEncodeSingle(cc1, array, index);
        } else {
          k++;
          if (k == uriLength) throw new $URIError("URI malformed");
          var cc2 = uri.charCodeAt(k);
          if (cc2 < 0xDC00 || cc2 > 0xDFFF) throw new $URIError("URI malformed");
          index = URIEncodePair(cc1, cc2, array, index);
        }
      }
    }

    var result = %NewString(array.length, NEW_ONE_BYTE_STRING);
    for (var i = 0; i < array.length; i++) {
      %_OneByteSeqStringSetChar(i, array[i], result);
    }
    return result;
  }

  // ECMA-262, section 15.1.3
  function Decode(uri, reserved) {
    var uriLength = uri.length;
    var one_byte = %NewString(uriLength, NEW_ONE_BYTE_STRING);
    var index = 0;
    var k = 0;

    // Optimistically assume one-byte string.
    for ( ; k < uriLength; k++) {
      var code = uri.charCodeAt(k);
      if (code == 37) {  // '%'
        if (k + 2 >= uriLength) throw new $URIError("URI malformed");
        var cc = URIHexCharsToCharCode(uri.charCodeAt(k+1), uri.charCodeAt(k+2));
        if (cc >> 7) break;  // Assumption wrong, two-byte string.
        if (reserved(cc)) {
          %_OneByteSeqStringSetChar(index++, 37, one_byte);  // '%'.
          %_OneByteSeqStringSetChar(index++, uri.charCodeAt(k+1), one_byte);
          %_OneByteSeqStringSetChar(index++, uri.charCodeAt(k+2), one_byte);
        } else {
          %_OneByteSeqStringSetChar(index++, cc, one_byte);
        }
        k += 2;
      } else {
        if (code > 0x7f) break;  // Assumption wrong, two-byte string.
        %_OneByteSeqStringSetChar(index++, code, one_byte);
      }
    }

    one_byte = %TruncateString(one_byte, index);
    if (k == uriLength) return one_byte;

    // Write into two byte string.
    var two_byte = %NewString(uriLength - k, NEW_TWO_BYTE_STRING);
    index = 0;

    for ( ; k < uriLength; k++) {
      var code = uri.charCodeAt(k);
      if (code == 37) {  // '%'
        if (k + 2 >= uriLength) throw new $URIError("URI malformed");
        var cc = URIHexCharsToCharCode(uri.charCodeAt(++k), uri.charCodeAt(++k));
        if (cc >> 7) {
          var n = 0;
          while (((cc << ++n) & 0x80) != 0) { }
          if (n == 1 || n > 4) throw new $URIError("URI malformed");
          var octets = new $Array(n);
          octets[0] = cc;
          if (k + 3 * (n - 1) >= uriLength) throw new $URIError("URI malformed");
          for (var i = 1; i < n; i++) {
            if (uri.charAt(++k) != '%') throw new $URIError("URI malformed");
            octets[i] = URIHexCharsToCharCode(uri.charCodeAt(++k),
                                              uri.charCodeAt(++k));
          }
          index = URIDecodeOctets(octets, two_byte, index);
        } else  if (reserved(cc)) {
          %_TwoByteSeqStringSetChar(index++, 37, two_byte);  // '%'.
          %_TwoByteSeqStringSetChar(index++, uri.charCodeAt(k - 1), two_byte);
          %_TwoByteSeqStringSetChar(index++, uri.charCodeAt(k), two_byte);
        } else {
          %_TwoByteSeqStringSetChar(index++, cc, two_byte);
        }
      } else {
        %_TwoByteSeqStringSetChar(index++, code, two_byte);
      }
    }

    two_byte = %TruncateString(two_byte, index);
    return one_byte + two_byte;
  }

  // -------------------------------------------------------------------
  // Define exported functions.

  // ECMA-262 - B.2.1.
  function URIEscapeJS(str) {
    var s = ToString(str);
    return %URIEscape(s);
  }

  // ECMA-262 - B.2.2.
  function URIUnescapeJS(str) {
    var s = ToString(str);
    return %URIUnescape(s);
  }

  // ECMA-262 - 15.1.3.1.
  function URIDecode(uri) {
    var reservedPredicate = function(cc) {
      // #$
      if (35 <= cc && cc <= 36) return true;
      // &
      if (cc == 38) return true;
      // +,
      if (43 <= cc && cc <= 44) return true;
      // /
      if (cc == 47) return true;
      // :;
      if (58 <= cc && cc <= 59) return true;
      // =
      if (cc == 61) return true;
      // ?@
      if (63 <= cc && cc <= 64) return true;

      return false;
    };
    var string = ToString(uri);
    return Decode(string, reservedPredicate);
  }

  // ECMA-262 - 15.1.3.2.
  function URIDecodeComponent(component) {
    var reservedPredicate = function(cc) { return false; };
    var string = ToString(component);
    return Decode(string, reservedPredicate);
  }

  // ECMA-262 - 15.1.3.3.
  function URIEncode(uri) {
    var unescapePredicate = function(cc) {
      if (isAlphaNumeric(cc)) return true;
      // !
      if (cc == 33) return true;
      // #$
      if (35 <= cc && cc <= 36) return true;
      // &'()*+,-./
      if (38 <= cc && cc <= 47) return true;
      // :;
      if (58 <= cc && cc <= 59) return true;
      // =
      if (cc == 61) return true;
      // ?@
      if (63 <= cc && cc <= 64) return true;
      // _
      if (cc == 95) return true;
      // ~
      if (cc == 126) return true;

      return false;
    };
    var string = ToString(uri);
    return Encode(string, unescapePredicate);
  }

  // ECMA-262 - 15.1.3.4
  function URIEncodeComponent(component) {
    var unescapePredicate = function(cc) {
      if (isAlphaNumeric(cc)) return true;
      // !
      if (cc == 33) return true;
      // '()*
      if (39 <= cc && cc <= 42) return true;
      // -.
      if (45 <= cc && cc <= 46) return true;
      // _
      if (cc == 95) return true;
      // ~
      if (cc == 126) return true;

      return false;
    };
    var string = ToString(component);
    return Encode(string, unescapePredicate);
  }

  // -------------------------------------------------------------------
  // Install exported functions.

  %CheckIsBootstrapping();

  // Set up non-enumerable URI functions on the global object and set
  // their names.
  InstallFunctions(global, DONT_ENUM, $Array(
      "escape", URIEscapeJS,
      "unescape", URIUnescapeJS,
      "decodeURI", URIDecode,
      "decodeURIComponent", URIDecodeComponent,
      "encodeURI", URIEncode,
      "encodeURIComponent", URIEncodeComponent
  ));

})();
