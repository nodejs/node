// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

// Query String Utilities

'use strict';

const { Buffer } = require('buffer');
const {
  hexTable,
  isHexTable
} = require('internal/querystring');
const QueryString = module.exports = {
  unescapeBuffer,
  // `unescape()` is a JS global, so we need to use a different local name
  unescape: qsUnescape,

  // `escape()` is a JS global, so we need to use a different local name
  escape: qsEscape,

  stringify,
  encode: stringify,

  parse,
  decode: parse
};

const unhexTable = [
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0 - 15
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 16 - 31
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 32 - 47
  +0, +1, +2, +3, +4, +5, +6, +7, +8, +9, -1, -1, -1, -1, -1, -1, // 48 - 63
  -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 64 - 79
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 80 - 95
  -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 96 - 111
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 112 - 127
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 128 ...
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1  // ... 255
];
// a safe fast alternative to decodeURIComponent
function unescapeBuffer(s, decodeSpaces) {
  var out = Buffer.allocUnsafe(s.length);
  var index = 0;
  var outIndex = 0;
  var currentChar;
  var nextChar;
  var hexHigh;
  var hexLow;
  var maxLength = s.length - 2;
  // Flag to know if some hex chars have been decoded
  var hasHex = false;
  while (index < s.length) {
    currentChar = s.charCodeAt(index);
    if (currentChar === 43 /*'+'*/ && decodeSpaces) {
      out[outIndex++] = 32; // ' '
      index++;
      continue;
    }
    if (currentChar === 37 /*'%'*/ && index < maxLength) {
      currentChar = s.charCodeAt(++index);
      hexHigh = unhexTable[currentChar];
      if (!(hexHigh >= 0)) {
        out[outIndex++] = 37; // '%'
      } else {
        nextChar = s.charCodeAt(++index);
        hexLow = unhexTable[nextChar];
        if (!(hexLow >= 0)) {
          out[outIndex++] = 37; // '%'
          out[outIndex++] = currentChar;
          currentChar = nextChar;
        } else {
          hasHex = true;
          currentChar = hexHigh * 16 + hexLow;
        }
      }
    }
    out[outIndex++] = currentChar;
    index++;
  }
  return hasHex ? out.slice(0, outIndex) : out;
}


function qsUnescape(s, decodeSpaces) {
  try {
    return decodeURIComponent(s);
  } catch (e) {
    return QueryString.unescapeBuffer(s, decodeSpaces).toString();
  }
}


// These characters do not need escaping when generating query strings:
// ! - . _ ~
// ' ( ) *
// digits
// alpha (uppercase)
// alpha (lowercase)
const noEscape = [
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0 - 15
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 16 - 31
  0, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, // 32 - 47
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, // 48 - 63
  0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 64 - 79
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, // 80 - 95
  0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 96 - 111
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0  // 112 - 127
];
// QueryString.escape() replaces encodeURIComponent()
// http://www.ecma-international.org/ecma-262/5.1/#sec-15.1.3.4
function qsEscape(str) {
  if (typeof str !== 'string') {
    if (typeof str === 'object')
      str = String(str);
    else
      str += '';
  }
  var out = '';
  var lastPos = 0;

  for (var i = 0; i < str.length; ++i) {
    var c = str.charCodeAt(i);

    // ASCII
    if (c < 0x80) {
      if (noEscape[c] === 1)
        continue;
      if (lastPos < i)
        out += str.slice(lastPos, i);
      lastPos = i + 1;
      out += hexTable[c];
      continue;
    }

    if (lastPos < i)
      out += str.slice(lastPos, i);

    // Multi-byte characters ...
    if (c < 0x800) {
      lastPos = i + 1;
      out += hexTable[0xC0 | (c >> 6)] + hexTable[0x80 | (c & 0x3F)];
      continue;
    }
    if (c < 0xD800 || c >= 0xE000) {
      lastPos = i + 1;
      out += hexTable[0xE0 | (c >> 12)] +
             hexTable[0x80 | ((c >> 6) & 0x3F)] +
             hexTable[0x80 | (c & 0x3F)];
      continue;
    }
    // Surrogate pair
    ++i;
    var c2;
    if (i < str.length)
      c2 = str.charCodeAt(i) & 0x3FF;
    else
      throw new URIError('URI malformed');
    lastPos = i + 1;
    c = 0x10000 + (((c & 0x3FF) << 10) | c2);
    out += hexTable[0xF0 | (c >> 18)] +
           hexTable[0x80 | ((c >> 12) & 0x3F)] +
           hexTable[0x80 | ((c >> 6) & 0x3F)] +
           hexTable[0x80 | (c & 0x3F)];
  }
  if (lastPos === 0)
    return str;
  if (lastPos < str.length)
    return out + str.slice(lastPos);
  return out;
}

function stringifyPrimitive(v) {
  if (typeof v === 'string')
    return v;
  if (typeof v === 'number' && isFinite(v))
    return '' + v;
  if (typeof v === 'boolean')
    return v ? 'true' : 'false';
  return '';
}


function stringify(obj, sep, eq, options) {
  sep = sep || '&';
  eq = eq || '=';

  var encode = QueryString.escape;
  if (options && typeof options.encodeURIComponent === 'function') {
    encode = options.encodeURIComponent;
  }

  if (obj !== null && typeof obj === 'object') {
    var keys = Object.keys(obj);
    var len = keys.length;
    var flast = len - 1;
    var fields = '';
    for (var i = 0; i < len; ++i) {
      var k = keys[i];
      var v = obj[k];
      var ks = encode(stringifyPrimitive(k)) + eq;

      if (Array.isArray(v)) {
        var vlen = v.length;
        var vlast = vlen - 1;
        for (var j = 0; j < vlen; ++j) {
          fields += ks + encode(stringifyPrimitive(v[j]));
          if (j < vlast)
            fields += sep;
        }
        if (vlen && i < flast)
          fields += sep;
      } else {
        fields += ks + encode(stringifyPrimitive(v));
        if (i < flast)
          fields += sep;
      }
    }
    return fields;
  }
  return '';
}

function charCodes(str) {
  if (str.length === 0) return [];
  if (str.length === 1) return [str.charCodeAt(0)];
  const ret = [];
  for (var i = 0; i < str.length; ++i)
    ret[ret.length] = str.charCodeAt(i);
  return ret;
}
const defSepCodes = [38]; // &
const defEqCodes = [61]; // =

// Parse a key/val string.
function parse(qs, sep, eq, options) {
  const obj = Object.create(null);

  if (typeof qs !== 'string' || qs.length === 0) {
    return obj;
  }

  var sepCodes = (!sep ? defSepCodes : charCodes(sep + ''));
  var eqCodes = (!eq ? defEqCodes : charCodes(eq + ''));
  const sepLen = sepCodes.length;
  const eqLen = eqCodes.length;

  var pairs = 1000;
  if (options && typeof options.maxKeys === 'number') {
    // -1 is used in place of a value like Infinity for meaning
    // "unlimited pairs" because of additional checks V8 (at least as of v5.4)
    // has to do when using variables that contain values like Infinity. Since
    // `pairs` is always decremented and checked explicitly for 0, -1 works
    // effectively the same as Infinity, while providing a significant
    // performance boost.
    pairs = (options.maxKeys > 0 ? options.maxKeys : -1);
  }

  var decode = QueryString.unescape;
  if (options && typeof options.decodeURIComponent === 'function') {
    decode = options.decodeURIComponent;
  }
  const customDecode = (decode !== qsUnescape);

  var lastPos = 0;
  var sepIdx = 0;
  var eqIdx = 0;
  var key = '';
  var value = '';
  var keyEncoded = customDecode;
  var valEncoded = customDecode;
  const plusChar = (customDecode ? '%20' : ' ');
  var encodeCheck = 0;
  for (var i = 0; i < qs.length; ++i) {
    const code = qs.charCodeAt(i);

    // Try matching key/value pair separator (e.g. '&')
    if (code === sepCodes[sepIdx]) {
      if (++sepIdx === sepLen) {
        // Key/value pair separator match!
        const end = i - sepIdx + 1;
        if (eqIdx < eqLen) {
          // We didn't find the (entire) key/value separator
          if (lastPos < end) {
            // Treat the substring as part of the key instead of the value
            key += qs.slice(lastPos, end);
          } else if (key.length === 0) {
            // We saw an empty substring between separators
            if (--pairs === 0)
              return obj;
            lastPos = i + 1;
            sepIdx = eqIdx = 0;
            continue;
          }
        } else {
          if (lastPos < end)
            value += qs.slice(lastPos, end);
        }

        if (key.length > 0 && keyEncoded)
          key = decodeStr(key, decode);
        if (value.length > 0 && valEncoded)
          value = decodeStr(value, decode);

        if (obj[key] === undefined) {
          obj[key] = value;
        } else {
          const curValue = obj[key];
          // A simple Array-specific property check is enough here to
          // distinguish from a string value and is faster and still safe
          // since we are generating all of the values being assigned.
          if (curValue.pop)
            curValue[curValue.length] = value;
          else
            obj[key] = [curValue, value];
        }
        if (--pairs === 0)
          return obj;
        keyEncoded = valEncoded = customDecode;
        key = value = '';
        encodeCheck = 0;
        lastPos = i + 1;
        sepIdx = eqIdx = 0;
      }
    } else {
      sepIdx = 0;
      // Try matching key/value separator (e.g. '=') if we haven't already
      if (eqIdx < eqLen) {
        if (code === eqCodes[eqIdx]) {
          if (++eqIdx === eqLen) {
            // Key/value separator match!
            const end = i - eqIdx + 1;
            if (lastPos < end)
              key += qs.slice(lastPos, end);
            encodeCheck = 0;
            lastPos = i + 1;
          }
          continue;
        } else {
          eqIdx = 0;
          if (!keyEncoded) {
            // Try to match an (valid) encoded byte once to minimize unnecessary
            // calls to string decoding functions
            if (code === 37/*%*/) {
              encodeCheck = 1;
              continue;
            } else if (encodeCheck > 0) {
              // eslint-disable-next-line no-extra-boolean-cast
              if (!!isHexTable[code]) {
                if (++encodeCheck === 3)
                  keyEncoded = true;
                continue;
              } else {
                encodeCheck = 0;
              }
            }
          }
        }
        if (code === 43/*+*/) {
          if (lastPos < i)
            key += qs.slice(lastPos, i);
          key += plusChar;
          lastPos = i + 1;
          continue;
        }
      }
      if (code === 43/*+*/) {
        if (lastPos < i)
          value += qs.slice(lastPos, i);
        value += plusChar;
        lastPos = i + 1;
      } else if (!valEncoded) {
        // Try to match an (valid) encoded byte (once) to minimize unnecessary
        // calls to string decoding functions
        if (code === 37/*%*/) {
          encodeCheck = 1;
        } else if (encodeCheck > 0) {
          // eslint-disable-next-line no-extra-boolean-cast
          if (!!isHexTable[code]) {
            if (++encodeCheck === 3)
              valEncoded = true;
          } else {
            encodeCheck = 0;
          }
        }
      }
    }
  }

  // Deal with any leftover key or value data
  if (lastPos < qs.length) {
    if (eqIdx < eqLen)
      key += qs.slice(lastPos);
    else if (sepIdx < sepLen)
      value += qs.slice(lastPos);
  } else if (eqIdx === 0 && key.length === 0) {
    // We ended on an empty substring
    return obj;
  }
  if (key.length > 0 && keyEncoded)
    key = decodeStr(key, decode);
  if (value.length > 0 && valEncoded)
    value = decodeStr(value, decode);
  if (obj[key] === undefined) {
    obj[key] = value;
  } else {
    const curValue = obj[key];
    // A simple Array-specific property check is enough here to
    // distinguish from a string value and is faster and still safe since
    // we are generating all of the values being assigned.
    if (curValue.pop)
      curValue[curValue.length] = value;
    else
      obj[key] = [curValue, value];
  }

  return obj;
}


// v8 does not optimize functions with try-catch blocks, so we isolate them here
// to minimize the damage (Note: no longer true as of V8 5.4 -- but still will
// not be inlined).
function decodeStr(s, decoder) {
  try {
    return decoder(s);
  } catch (e) {
    return QueryString.unescape(s, true);
  }
}
