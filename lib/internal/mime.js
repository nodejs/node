'use strict';
const {
  FunctionPrototypeCall,
  ObjectDefineProperty,
  SafeMap,
  SafeStringPrototypeSearch,
  StringPrototypeTrimEnd,
  StringPrototypeTrim,
  StringPrototypeCharCodeAt,
  StringPrototypeIndexOf,
  StringPrototypeSlice,
  StringPrototypeToLowerCase,
  SymbolIterator,
} = primordials;
const {
  ERR_INVALID_MIME_SYNTAX,
} = require('internal/errors').codes;

const NOT_HTTP_TOKEN_CODE_POINT = /[^!#$%&'*+\-.^_`|~A-Za-z0-9]/;
const NOT_HTTP_QUOTED_STRING_CODE_POINT = /[^\t\u0020-~\u0080-\u00FF]/;

const SOLIDUS = '/';
const WHITESPACE_REGEX = /[^\r\n\t ]/;
const SEMICOLON = ';';
const BACKSLASH = '\\';
const QUOTE = '"';
const ASCII_LOWER_LOWERBOUND = StringPrototypeCharCodeAt('A');
const ASCII_LOWER_UPPERBOUND = StringPrototypeCharCodeAt('Z');
const SEMICOLON_CHARCODE = StringPrototypeCharCodeAt(SEMICOLON);
const EQUALS_CHARCODE = StringPrototypeCharCodeAt('=');
const BACKSLASH_CHARCODE = StringPrototypeCharCodeAt(BACKSLASH);
const QUOTE_CHARCODE = StringPrototypeCharCodeAt(QUOTE);
function toASCIILower(str) {
  let result = '';
  for (let i = 0; i < str.length; i++) {
    const char = str[i];
    const charCode = StringPrototypeCharCodeAt(char, 0);

    result += charCode >= ASCII_LOWER_LOWERBOUND &&
      charCode <= ASCII_LOWER_UPPERBOUND ?
      StringPrototypeToLowerCase(char) :
      char;
  }
  return result;
}

// https://fetch.spec.whatwg.org/#collect-an-http-quoted-string
function collectHTTPQuotedString(str, marker) {
  const limit = str.length;
  let index = marker.position;
  // #2 Let value be the empty string.
  let value = '';

  // #4 Advance position by 1.
  index++;

  // #5 While true
  while (true) {
    let nextDelimiter = StringPrototypeIndexOf(str, BACKSLASH, index);

    nextDelimiter = nextDelimiter !== -1 ? nextDelimiter : StringPrototypeIndexOf(str, QUOTE, index);

    // #5-1 Append the result of collecting a sequence of code points
    // that are not U+0022 (") or U+005C (\) from input, given position, to value.
    const collectedPoints = nextDelimiter === -1 ?
      StringPrototypeSlice(str, index) : StringPrototypeSlice(str, index, nextDelimiter);
    index += collectedPoints.length;
    value += collectedPoints;

    // #5-2 If position is past the end of input, then break.
    if (index > limit) break;

    // #5-3 Let quoteOrBackslash be the code point at position within input.
    const quoteOrBackslash = str[index];
    const quoteOrBackslashAsCode = quoteOrBackslash == null ?
      0 :
      StringPrototypeCharCodeAt(quoteOrBackslash, 0);

    // #5-4 Advance position by 1.
    index++;

    // #5-5 If quoteOrBackslash is U+005C (\), then:
    if (quoteOrBackslashAsCode === BACKSLASH_CHARCODE) {
      // #5-5-1 If position is past the end of input, then append U+005C (\) to value and break.
      if (index >= limit) {
        value += BACKSLASH;
        break;
      }

      // #5-5-2 Append the code point at position within input to value.
      value += str[index];

      // #5-5-3 Advance position by 1.
      index++;
    } else if (quoteOrBackslashAsCode === QUOTE_CHARCODE) {
      // #5-6 Otherwise
      // #5-6-1 Assert: quoteOrBackslash is U+0022 (").
      // #5-6-2 Break
      break;
    }
  }

  marker.position = index;

  // #6 If the extract-value flag is set, then return value
  return value;
}

function parseTypeAndSubtype(str) {
  const strLength = str.length;
  // #2 Let position be a position variable for input, initially pointg at
  // the start of the input
  let position = 0;

  // #3 Let type be the result of collecting a sequence of code points that are
  // not U+002F (/ [SOLIDUS]) from input, given position.
  const typeEnd = StringPrototypeIndexOf(str, SOLIDUS);
  const preType = typeEnd === -1 ?
    str :
    StringPrototypeSlice(str, position, typeEnd);

  // #4 If type is the empty string or does not solely
  // contain HTTP token code points, then return failure.
  // #5 If position is past the end of input, then return failure.
  if (typeEnd === -1 || preType.length === 0 || typeEnd > strLength || SafeStringPrototypeSearch(
    preType, NOT_HTTP_TOKEN_CODE_POINT) !== -1) {
    throw new ERR_INVALID_MIME_SYNTAX('type', str, typeEnd);
  }

  // #6 Advance position by 1. (This skips past U+002F (/).)
  position = typeEnd + 1;

  // #7 Let subtype be the result of collecting a sequence of code points
  // that are not U+003B (;) from input, given position.
  let subtypeEnd = StringPrototypeIndexOf(str, SEMICOLON, position);
  subtypeEnd = subtypeEnd === -1 ? strLength : subtypeEnd;
  const rawSubtype = StringPrototypeSlice(str, position, subtypeEnd);
  position += rawSubtype.length;

  // #8 Remove any trailing HTTP whitespace from subtype.
  const preSubType = StringPrototypeTrimEnd(rawSubtype);

  // #9 If subtype is the empty string or does not solely contain
  // HTTP token code points, then return failure.
  if (preSubType.length === 0 ||
    SafeStringPrototypeSearch(preSubType,
                              NOT_HTTP_TOKEN_CODE_POINT) !== -1) {
    throw new ERR_INVALID_MIME_SYNTAX('subtype', str, subtypeEnd);
  }

  const type = toASCIILower(preType);
  const subtype = toASCIILower(preSubType);
  return {
    __proto__: null,
    type,
    subtype,
    position,
  ];
}

function isSemicolonOrEquals(char) {
  const code = StringPrototypeCharCodeAt(char, 0);
  return code === SEMICOLON_CHARCODE || code === EQUALS_CHARCODE;
}

function moveUntil(str, predicate, marker, {
  toASCII,
  removeBackslashes,
} = {
  toASCII: false,
  removeBackslashes: false,
},
) {
  const start = marker.position;
  const length = str.length;
  let index = start;
  let result = '';
  let char = str[index];

  while (index < length && predicate(char) === false) {
    let asCharCode;
    let controlCharCode;
    // We lazily assign it if needed, as not all the iterations
    // might require it
    if (removeBackslashes && (
      asCharCode = StringPrototypeCharCodeAt(char, 0)
    ) === BACKSLASH_CHARCODE
    ) {
      // Jump to the next char
      index++;
      char = str[index];
      controlCharCode = StringPrototypeCharCodeAt(char, 0);
    }

    if (toASCII) {
      asCharCode = controlCharCode != null ?
        controlCharCode :
        asCharCode || StringPrototypeCharCodeAt(char, 0);
      result += asCharCode >= ASCII_LOWER_LOWERBOUND &&
        asCharCode <= ASCII_LOWER_UPPERBOUND ?
        StringPrototypeToLowerCase(char) :
        char;
    } else {
      result += char;
    }

    index++;
    char = str[index];
  }

  marker.position = index;

  return result;
}

function escapeQuoteOrSolidus(str) {
  let result = '';

  for (let i = 0; i < str.length; i++) {
    const char = str[i];
    const code = StringPrototypeCharCodeAt(char, 0);
    result += (code === QUOTE_CHARCODE || char === BACKSLASH_CHARCODE) ? `\\${char}` : char;
  }
  return result;
}

const encode = (value) => {
  if (value.length === 0) return '""';
  const shouldEncode = SafeStringPrototypeSearch(value, NOT_HTTP_TOKEN_CODE_POINT) !== -1;
  if (!shouldEncode) return value;
  const escaped = escapeQuoteOrSolidus(value);
  return `"${escaped}"`;
};

class MIMEParams {
  #data = new SafeMap();
  // We set the flag the MIMEParams instance as processed on initialization
  // to defer the parsing of a potentially large string.
  #processed = true;
  #string = null;

  /**
   * Used to instantiate a MIMEParams object within the MIMEType class and
   * to allow it to be parsed lazily.
   */
  static instantiateMimeParams(str) {
    const instance = new MIMEParams();
    instance.#string = str;
    instance.#processed = false;
    return instance;
  }

  delete(name) {
    this.#parse();
    this.#data.delete(name);
  }

  get(name) {
    return this.#data.get(name) ?? null;
  }

  has(name) {
    this.#parse();
    return this.#data.has(name);
  }

  set(name, value) {
    this.#parse();
    const data = this.#data;
    name = `${name}`;
    value = `${value}`;
    let invalidNameIndex = -1;

    // String.prototype.search is needed if we want to
    // explicitly indicate to the end-user where the error is located
    if (name.length === 0 ||
      (invalidNameIndex = SafeStringPrototypeSearch(name, NOT_HTTP_TOKEN_CODE_POINT)
      , invalidNameIndex) !== -1
    ) {
      throw new ERR_INVALID_MIME_SYNTAX(
        'parameter name',
        name,
        invalidNameIndex,
      );
    }

    let invalidValueIndex = -1;
    if (value.length > 0 &&
      (invalidValueIndex = SafeStringPrototypeSearch(value, NOT_HTTP_QUOTED_STRING_CODE_POINT),
      invalidValueIndex) !== -1
    ) {
      throw new ERR_INVALID_MIME_SYNTAX(
        'parameter value',
        value,
        invalidValueIndex,
      );
    }

    data.set(name, value);
  }

  *entries() {
    this.#parse();
    yield* this.#data.entries();
  }

  *keys() {
    this.#parse();
    yield* this.#data.keys();
  }

  *values() {
    this.#parse();
    yield* this.#data.values();
  }

  toString() {
    this.#parse();
    let ret = '';
    for (const { 0: key, 1: value } of this.#data) {
      const encoded = encode(value);
      // Ensure they are separated
      if (ret.length !== 0) ret += ';';
      ret += `${key}=${encoded}`;
    }

    return ret;
  }

  // Used to act as a friendly class to stringifying stuff
  // not meant to be exposed to users, could inject invalid values
  static parseParametersString(str, position, params) {
    const paramsMap = params.#data;
    const endOfSource = str.length;
    const marker = { position };

    // #11 While position is not past the end of input:
    while (marker.position < endOfSource) {

      // #11-2 Advance position by 1. (This skips past U+003B (; [SEMICOLON]).)
      marker.position++;

      // #11-3 Collect a sequence of code points that are HTTP whitespace from input given position.
      const sliced = StringPrototypeSlice(str, marker.position);
      const endOfWhitespace = SafeStringPrototypeSearch(sliced, WHITESPACE_REGEX);
      marker.position += endOfWhitespace;

      const parameterName = moveUntil(str, isSemicolonOrEquals, marker, { toASCII: true });

      // #11-6 If position is past the end of input, then break.
      if (marker.position > endOfSource) break;
      // #11-5-1 If the code point at position within input is U+003B (;), then continue.
      if (StringPrototypeCharCodeAt(str, marker.position) === SEMICOLON_CHARCODE) {
        continue;
      }

      // #11-5-2 Advance position by 1. (This skips past U+003D (=).)
      marker.position++;

      const nextChar = StringPrototypeCharCodeAt(str, marker.position);

      // #11-7 Let parameterValue be null.
      let parameterValue = null;
      // #11-8 If the code point at position within input is U+0022 ("), then:
      if (nextChar === QUOTE_CHARCODE) {
        // #11-8-1 If the code point at position within input is U+0022 ("), then:
        // Unescape '\' quoted characters
        parameterValue = collectHTTPQuotedString(str, marker);

        // #11-8-1 Collect a sequence of code points that are not U+003B (;) from input, given position.
        const remaining = StringPrototypeIndexOf(str, SEMICOLON, marker.position);
        marker.position += remaining !== -1 ? remaining - marker.position : 0;
      } else {
        // #11-9-1 Set parameterValue to the result of
        // collecting a sequence of code points that are not U+003B (;) from input,
        // given position.
        const valueEnd = StringPrototypeIndexOf(str, SEMICOLON, marker.position);
        const rawValue = valueEnd === -1 ?
          StringPrototypeSlice(str, marker.position) :
          StringPrototypeSlice(str, marker.position, valueEnd);

        // #11-9-2 Remove any trailing HTTP whitespace from parameterValue.
        const trimmedValue = StringPrototypeTrimEnd(rawValue);
        marker.position += trimmedValue.length;

        // #11-9-3 If parameterValue is the empty string, then continue.
        if (trimmedValue.length === 0) continue;

        parameterValue = trimmedValue;
      }

      /**
       * #11-10 if:
       * parameterName is not the empty string
       * parameterName solely contains HTTP token code points
       * parameterValue solely contains HTTP quoted-string token code points
       * mimeType parameters[parameterName] does not exist
       * then set mimeType parameters[parameterName] to parameterValue.
       */
      if (
        parameterName.length !== 0 &&
        SafeStringPrototypeSearch(
          parameterName,
          NOT_HTTP_TOKEN_CODE_POINT,
        ) === -1 &&
        SafeStringPrototypeSearch(
          parameterValue,
          NOT_HTTP_QUOTED_STRING_CODE_POINT,
        ) === -1 &&
        params.has(parameterName) === false
      ) {
        paramsMap.set(parameterName, parameterValue);
      }
    }

    return paramsMap;
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
  #essence;
  constructor(string) {
    // #1 Remove any leading and trailing HTTP whitespace from input
    string = StringPrototypeTrim(`${string}`);

    // #10 Let mimeType be a new MIME type record whose type is type,
    // in ASCII lowercase, and subtype is subtype, in ASCII lowercase.
    const { type, subtype, parametersStringIndex } = parseTypeAndSubtype(string);
    this.#type = type;
    this.#subtype = subtype;
    this.#essence = `${this.#type}/${this.#subtype}`;
    this.#parameters = new MIMEParams();
    parseParametersString(
      string,
      parametersStringIndex,
      this.#parameters,
    );
  }

  get type() {
    return this.#type;
  }

  set type(v) {
    v = `${v}`;
    const invalidTypeIndex = SafeStringPrototypeSearch(v, NOT_HTTP_TOKEN_CODE_POINT);
    if (invalidTypeIndex !== -1 || v.length === 0) {
      throw new ERR_INVALID_MIME_SYNTAX('type', v, invalidTypeIndex);
    }
    this.#type = toASCIILower(v);
    this.#essence = `${this.#type}/${this.#subtype}`;
  }

  get subtype() {
    return this.#subtype;
  }

  set subtype(v) {
    v = `${v}`;
    const invalidTypeIndex = SafeStringPrototypeSearch(v, NOT_HTTP_TOKEN_CODE_POINT);
    if (invalidTypeIndex !== -1 || v.length === 0) {
      throw new ERR_INVALID_MIME_SYNTAX('subtype', v, invalidTypeIndex);
    }
    this.#subtype = toASCIILower(v);
    this.#essence = `${this.#type}/${this.#subtype}`;
  }

  get essence() {
    return this.#essence;
  }

  get params() {
    return this.#parameters;
  }

  toString() {
    let ret = this.#essence;
    const paramStr = FunctionPrototypeCall(MIMEParamsStringify, this.#parameters);
    if (paramStr.length > 0) ret += `;${paramStr}`;
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
