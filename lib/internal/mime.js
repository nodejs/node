'use strict';

const {
  FunctionPrototypeCall,
  ObjectDefineProperty,
  SafeMap,
  StringPrototypeCharCodeAt,
  StringPrototypeSlice,
  StringPrototypeToLowerCase,
  Symbol,
  SymbolIterator,
  Uint8Array,
} = primordials;
const {
  ERR_ILLEGAL_CONSTRUCTOR,
  ERR_INVALID_MIME_SYNTAX,
} = require('internal/errors').codes;

// Lookup table for the code point classes used by the MIME Sniffing
// standard. Only code points <= 0xFF can be in either class.
// https://mimesniff.spec.whatwg.org/#http-token-code-point
// https://mimesniff.spec.whatwg.org/#http-quoted-string-token-code-point
const kHTTPToken = 1;
const kHTTPQuotedStringToken = 2;
const codePointClass = new Uint8Array(256);
{
  codePointClass[0x09] = kHTTPQuotedStringToken;
  for (let c = 0x20; c <= 0x7E; c++) codePointClass[c] = kHTTPQuotedStringToken;
  for (let c = 0x80; c <= 0xFF; c++) codePointClass[c] = kHTTPQuotedStringToken;
  const tokens = "!#$%&'*+-.^_`|~0123456789" +
    'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz';
  for (let i = 0; i < tokens.length; i++) {
    codePointClass[StringPrototypeCharCodeAt(tokens, i)] |= kHTTPToken;
  }
}

const kNoThrow = Symbol('kNoThrow');

// https://fetch.spec.whatwg.org/#http-whitespace
function isHTTPWhitespace(c) {
  return c === 0x20 || c === 0x09 || c === 0x0A || c === 0x0D;
}

/**
 * Returns the offset from `start` of the first code point in
 * `str[start, end)` that is not in `cls`, or -1 if they all are.
 * @param {string} str
 * @param {number} start
 * @param {number} end
 * @param {number} cls
 * @returns {number}
 */
function findInvalid(str, start, end, cls) {
  for (let i = start; i < end; i++) {
    const c = StringPrototypeCharCodeAt(str, i);
    if (c > 0xFF || (codePointClass[c] & cls) === 0) return i - start;
  }
  return -1;
}

function toASCIILower(str) {
  let hasUpper = false;
  for (let i = 0; i < str.length; i++) {
    const c = StringPrototypeCharCodeAt(str, i);
    if (c > 0x7F) {
      let result = '';
      for (let j = 0; j < str.length; j++) {
        const char = str[j];
        result += char >= 'A' && char <= 'Z' ?
          StringPrototypeToLowerCase(char) :
          char;
      }
      return result;
    }
    if (c <= 0x5A && c >= 0x41) hasUpper = true;
  }
  // Returning the input unchanged when possible avoids an allocation and
  // keeps its cached hash for Map lookups.
  return hasUpper ? StringPrototypeToLowerCase(str) : str;
}

// Results of parseTypeAndSubtype(). Kept in module state to avoid allocating
// a result object per parse; they are consumed synchronously by the caller.
let parsedType = '';
let parsedSubtype = '';
let parsedParamsStart = 0;
let parsedParamsEnd = 0;
let failedProduction = '';
let failedIndex = -1;

/**
 * Parses the type and subtype of a MIME type string and locates its
 * parameters. Returns false on failure, with failedProduction and
 * failedIndex describing the error.
 * @see https://mimesniff.spec.whatwg.org/#parse-a-mime-type
 * @param {string} str
 * @returns {boolean}
 */
function parseTypeAndSubtype(str) {
  const length = str.length;

  // Skip leading HTTP whitespace.
  let position = 0;
  while (position < length &&
         isHTTPWhitespace(StringPrototypeCharCodeAt(str, position))) {
    position++;
  }

  // Collect the type, up to '/'.
  const typeStart = position;
  let invalidTypeIndex = -1;
  let typeHasUpper = false;
  for (; position < length; position++) {
    const c = StringPrototypeCharCodeAt(str, position);
    if (c === 0x2F /* / */) break;
    if (c > 0xFF || (codePointClass[c] & kHTTPToken) === 0) {
      if (invalidTypeIndex === -1) invalidTypeIndex = position - typeStart;
    } else if (c <= 0x5A && c >= 0x41) {
      typeHasUpper = true;
    }
  }
  if (position === typeStart || invalidTypeIndex !== -1 || position >= length) {
    failedProduction = 'type';
    failedIndex = invalidTypeIndex;
    return false;
  }
  const typeEnd = position;

  // Skip '/' and collect the subtype, up to ';'. In the same pass, find the
  // first non-token code point and where the trailing HTTP whitespace starts.
  position++;
  const subtypeStart = position;
  let subtypeEnd = position;
  let firstNonToken = -1;
  let subtypeHasUpper = false;
  for (; position < length; position++) {
    const c = StringPrototypeCharCodeAt(str, position);
    if (c === 0x3B /* ; */) break;
    if (c <= 0xFF && (codePointClass[c] & kHTTPToken) !== 0) {
      if (c <= 0x5A && c >= 0x41) subtypeHasUpper = true;
    } else if (firstNonToken === -1) {
      firstNonToken = position;
    }
    if (!isHTTPWhitespace(c)) subtypeEnd = position + 1;
  }
  const subtypeRawEnd = position;
  // Whitespace is not a token code point, so a non-token code point at or
  // after subtypeEnd is part of the trailing whitespace, which is removed.
  const invalidSubtypeIndex = firstNonToken !== -1 && firstNonToken < subtypeEnd ?
    firstNonToken - subtypeStart : -1;
  if (subtypeEnd === subtypeStart || invalidSubtypeIndex !== -1) {
    failedProduction = 'subtype';
    failedIndex = invalidSubtypeIndex;
    return false;
  }

  // Parameters are everything after the ';' up to the trailing whitespace.
  let paramsEnd = length;
  while (paramsEnd > subtypeRawEnd &&
         isHTTPWhitespace(StringPrototypeCharCodeAt(str, paramsEnd - 1))) {
    paramsEnd--;
  }

  const type = StringPrototypeSlice(str, typeStart, typeEnd);
  const subtype = StringPrototypeSlice(str, subtypeStart, subtypeEnd);
  // Both are ASCII-only at this point, so toLowerCase() is ASCII lowercase.
  parsedType = typeHasUpper ? StringPrototypeToLowerCase(type) : type;
  parsedSubtype = subtypeHasUpper ? StringPrototypeToLowerCase(subtype) : subtype;
  parsedParamsStart = subtypeRawEnd + 1;
  parsedParamsEnd = paramsEnd;
  return true;
}

/**
 * Parses MIME type parameters from `str[position, end)` into `params`.
 * `position` is just past the ';' that follows the subtype, and `end`
 * excludes trailing HTTP whitespace. This is step 11 of
 * https://mimesniff.spec.whatwg.org/#parse-a-mime-type
 * @param {string} str
 * @param {number} position
 * @param {number} end
 * @param {SafeMap<string, string>} params
 */
function parseParameters(str, position, end, params) {
  while (position < end) {
    // Skip HTTP whitespace.
    while (position < end &&
           isHTTPWhitespace(StringPrototypeCharCodeAt(str, position))) {
      position++;
    }

    // Collect the parameter name, up to ';' or '='.
    const nameStart = position;
    let nameIsToken = true;
    let nameHasUpper = false;
    for (; position < end; position++) {
      const c = StringPrototypeCharCodeAt(str, position);
      if (c === 0x3B /* ; */ || c === 0x3D /* = */) break;
      if (c > 0xFF || (codePointClass[c] & kHTTPToken) === 0) {
        nameIsToken = false;
      } else if (c <= 0x5A && c >= 0x41) {
        nameHasUpper = true;
      }
    }
    const nameEnd = position;

    if (position < end) {
      // Parameters without a value are ignored.
      if (StringPrototypeCharCodeAt(str, position) === 0x3B /* ; */) {
        position++;
        continue;
      }
      // Skip '='.
      position++;
    }
    if (position >= end) break;

    let value;
    if (StringPrototypeCharCodeAt(str, position) === 0x22 /* " */) {
      // Collect an HTTP quoted string with the extract-value flag.
      // https://fetch.spec.whatwg.org/#collect-an-http-quoted-string
      position++;
      value = '';
      let chunkStart = position;
      while (true) {
        while (position < end) {
          const c = StringPrototypeCharCodeAt(str, position);
          if (c === 0x22 /* " */ || c === 0x5C /* \ */) break;
          position++;
        }
        if (position >= end) {
          value += StringPrototypeSlice(str, chunkStart, position);
          break;
        }
        const quoteOrBackslash = StringPrototypeCharCodeAt(str, position);
        value += StringPrototypeSlice(str, chunkStart, position);
        position++;
        if (quoteOrBackslash === 0x5C /* \ */) {
          if (position >= end) {
            value += '\\';
            break;
          }
          // The escaped code point starts the next chunk.
          chunkStart = position;
          position++;
        } else {
          break;
        }
      }
      // Skip anything else up to the next ';'.
      while (position < end &&
             StringPrototypeCharCodeAt(str, position) !== 0x3B /* ; */) {
        position++;
      }
      if (findInvalid(value, 0, value.length, kHTTPQuotedStringToken) !== -1) {
        position++;
        continue;
      }
    } else {
      // Collect the value up to ';', validating it and finding where its
      // trailing HTTP whitespace starts in the same pass. CR and LF are not
      // quoted-string token code points, but trailing ones are removed, so
      // the value is only invalid if a bad code point comes before valueEnd.
      const valueStart = position;
      let valueEnd = position;
      let firstInvalid = -1;
      for (; position < end; position++) {
        const c = StringPrototypeCharCodeAt(str, position);
        if (c === 0x3B /* ; */) break;
        if (firstInvalid === -1 &&
            (c > 0xFF || (codePointClass[c] & kHTTPQuotedStringToken) === 0)) {
          firstInvalid = position;
        }
        if (!isHTTPWhitespace(c)) valueEnd = position + 1;
      }
      // Parameters with an empty or invalid value are ignored.
      if (valueEnd === valueStart ||
          (firstInvalid !== -1 && firstInvalid < valueEnd)) {
        position++;
        continue;
      }
      value = StringPrototypeSlice(str, valueStart, valueEnd);
    }

    if (nameEnd !== nameStart && nameIsToken) {
      // The name is ASCII-only, so toLowerCase() is ASCII lowercase.
      const name = nameHasUpper ?
        StringPrototypeToLowerCase(StringPrototypeSlice(str, nameStart, nameEnd)) :
        StringPrototypeSlice(str, nameStart, nameEnd);
      if (!params.has(name)) params.set(name, value);
    }
    // Skip ';'.
    position++;
  }
}

function escapeQuoteOrSolidus(str) {
  let result = '';
  for (let i = 0; i < str.length; i++) {
    const char = str[i];
    result += (char === '"' || char === '\\') ? `\\${char}` : char;
  }
  return result;
}

const encode = (value) => {
  if (value.length === 0) return '""';
  if (findInvalid(value, 0, value.length, kHTTPToken) === -1) return value;
  return `"${escapeQuoteOrSolidus(value)}"`;
};

class MIMEParams {
  // Parsing is deferred until the parameters are first accessed, as most
  // users only need the type and subtype. Until then #data is null and
  // #string[#start, #end) holds the unparsed parameters.
  #data = null;
  #string = null;
  #start = 0;
  #end = 0;

  /**
   * Used to instantiate a MIMEParams object within the MIMEType class and
   * to allow it to be parsed lazily.
   * @returns {MIMEParams}
   */
  static instantiateMimeParams(str, start, end) {
    const instance = new MIMEParams();
    instance.#string = str;
    instance.#start = start;
    instance.#end = end;
    return instance;
  }

  /**
   * @param {string} name
   * @returns {void}
   */
  delete(name) {
    this.#parse().delete(toASCIILower(`${name}`));
  }

  get(name) {
    const value = this.#parse().get(toASCIILower(`${name}`));
    return value === undefined ? null : value;
  }

  has(name) {
    return this.#parse().has(toASCIILower(`${name}`));
  }

  set(name, value) {
    const data = this.#parse();
    name = toASCIILower(`${name}`);
    value = `${value}`;
    const invalidNameIndex = findInvalid(name, 0, name.length, kHTTPToken);
    if (name.length === 0 || invalidNameIndex !== -1) {
      throw new ERR_INVALID_MIME_SYNTAX(
        'parameter name',
        name,
        invalidNameIndex,
      );
    }
    const invalidValueIndex = findInvalid(value, 0, value.length,
                                          kHTTPQuotedStringToken);
    if (invalidValueIndex !== -1) {
      throw new ERR_INVALID_MIME_SYNTAX(
        'parameter value',
        value,
        invalidValueIndex,
      );
    }
    data.set(name, value);
  }

  *entries() {
    yield* this.#parse().entries();
  }

  *keys() {
    yield* this.#parse().keys();
  }

  *values() {
    yield* this.#parse().values();
  }

  toString() {
    let ret = '';
    for (const { 0: key, 1: value } of this.#parse()) {
      const encoded = encode(value);
      // Ensure they are separated
      if (ret.length) ret += ';';
      ret += `${key}=${encoded}`;
    }
    return ret;
  }

  /**
   * Parses the deferred parameter string on first use.
   * @returns {SafeMap<string, string>}
   */
  #parse() {
    let data = this.#data;
    if (data !== null) return data;
    data = this.#data = new SafeMap();
    const str = this.#string;
    if (str !== null) {
      this.#string = null;
      parseParameters(str, this.#start, this.#end, data);
    }
    return data;
  }
}
const MIMEParamsStringify = MIMEParams.prototype.toString;
ObjectDefineProperty(MIMEParams.prototype, SymbolIterator, {
  __proto__: null,
  configurable: true,
  value: MIMEParams.prototype.entries,
  writable: true,
});
ObjectDefineProperty(MIMEParams.prototype, 'toJSON', {
  __proto__: null,
  configurable: true,
  value: MIMEParamsStringify,
  writable: true,
});

const { instantiateMimeParams } = MIMEParams;
delete MIMEParams.instantiateMimeParams;

class MIMEType {
  #type;
  #subtype;
  #parameters;
  constructor(string, noThrowSymbol = null) {
    // MIMEType.parse() has already parsed the string successfully.
    if (noThrowSymbol === kNoThrow) {
      this.#init(string);
      return;
    }
    string = `${string}`;
    if (noThrowSymbol != null) {
      throw new ERR_ILLEGAL_CONSTRUCTOR();
    }
    if (!parseTypeAndSubtype(string)) {
      throw new ERR_INVALID_MIME_SYNTAX(failedProduction, string, failedIndex);
    }
    this.#init(string);
  }

  // Consumes the results of a successful parseTypeAndSubtype(string).
  #init(string) {
    this.#type = parsedType;
    this.#subtype = parsedSubtype;
    this.#parameters = instantiateMimeParams(string, parsedParamsStart,
                                             parsedParamsEnd);
  }

  // Like the constructor, but returns null instead of throwing on invalid input.
  static parse(string) {
    string = `${string}`;
    if (!parseTypeAndSubtype(string)) return null;
    return new MIMEType(string, kNoThrow);
  }

  get type() {
    return this.#type;
  }

  set type(v) {
    v = `${v}`;
    const invalidTypeIndex = findInvalid(v, 0, v.length, kHTTPToken);
    if (v.length === 0 || invalidTypeIndex !== -1) {
      throw new ERR_INVALID_MIME_SYNTAX('type', v, invalidTypeIndex);
    }
    this.#type = toASCIILower(v);
  }

  get subtype() {
    return this.#subtype;
  }

  set subtype(v) {
    v = `${v}`;
    const invalidSubtypeIndex = findInvalid(v, 0, v.length, kHTTPToken);
    if (v.length === 0 || invalidSubtypeIndex !== -1) {
      throw new ERR_INVALID_MIME_SYNTAX('subtype', v, invalidSubtypeIndex);
    }
    this.#subtype = toASCIILower(v);
  }

  get essence() {
    return `${this.#type}/${this.#subtype}`;
  }

  get params() {
    return this.#parameters;
  }

  toString() {
    let ret = `${this.#type}/${this.#subtype}`;
    const paramStr = FunctionPrototypeCall(MIMEParamsStringify, this.#parameters);
    if (paramStr.length) ret += `;${paramStr}`;
    return ret;
  }
}
ObjectDefineProperty(MIMEType.prototype, 'toJSON', {
  __proto__: null,
  configurable: true,
  value: MIMEType.prototype.toString,
  writable: true,
});

module.exports = {
  MIMEParams,
  MIMEType,
};
