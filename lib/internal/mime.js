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
  RegExpPrototypeTest,
  SymbolIterator,
} = primordials;
const {
  ERR_INVALID_MIME_SYNTAX,
} = require('internal/errors').codes;

const NOT_HTTP_TOKEN_CODE_POINT = /[^!#$%&'*+\-.^_`|~A-Za-z0-9]/;
const NOT_HTTP_QUOTED_STRING_CODE_POINT = /[^\t\u0020-~\u0080-\u00FF]/;

const WHITESPACE_REGEX = /[\r\n\t ]/;
const ASCII_LOWER_LOWERBOUND = StringPrototypeCharCodeAt('A');
const ASCII_LOWER_UPPERBOUND = StringPrototypeCharCodeAt('Z');
const SEMICOLON_CHARCODE = StringPrototypeCharCodeAt(';');
const EQUALS_CHARCODE = StringPrototypeCharCodeAt('=');
const BACKSLASH_CHARCODE = StringPrototypeCharCodeAt('\\');
const QUOTE_CHARCODE = StringPrototypeCharCodeAt('"');
const SOLIDUS = '/';
const SEMICOLON = ';';

function toASCIILower(str) {
  let result = '';
  for (let i = 0; i < str.length; i++) {
    const char = str[i];
    const charCode = StringPrototypeCharCodeAt(str[i], 0);

    result += charCode >= ASCII_LOWER_LOWERBOUND &&
      charCode <= ASCII_LOWER_UPPERBOUND ?
        StringPrototypeToLowerCase(charCode) :
        char;
  }
  return result;
}


function parseTypeAndSubtype(str) {
  // Skip only HTTP whitespace from start
  str = StringPrototypeTrim(str);
  let position = 0;
  // read until '/'
  const typeEnd = StringPrototypeIndexOf(str, SOLIDUS);
  const mimetype = typeEnd === -1 ?
    str :
    StringPrototypeSlice(str, position, typeEnd);
  if (typeEnd === -1 || mimetype.length === 0 || RegExpPrototypeTest(
    NOT_HTTP_TOKEN_CODE_POINT,
    mimetype)) {
    throw new ERR_INVALID_MIME_SYNTAX('type', str, typeEnd);
  }
  // skip type and '/'
  position = typeEnd + 1;
  const type = toASCIILower(mimetype);
  // read until ';'
  const subtypeEnd = StringPrototypeIndexOf(str, SEMICOLON, position);
  const rawSubtype = subtypeEnd === -1 ?
    str :
    StringPrototypeSlice(str, position, subtypeEnd);
  position += rawSubtype.length;
  if (subtypeEnd !== -1) {
    // skip ';'
    position += 1;
  }
  const trimmedSubtype = StringPrototypeTrimEnd(rawSubtype);
  if (trimmedSubtype.length === 0 ||
      RegExpPrototypeTest(NOT_HTTP_TOKEN_CODE_POINT,
                          trimmedSubtype)) {
    throw new ERR_INVALID_MIME_SYNTAX('subtype', str, trimmedSubtype);
  }
  const subtype = toASCIILower(trimmedSubtype);
  return {
    __proto__: null,
    type,
    subtype,
    parametersStringIndex: position,
  };
}

function isSemicolonOrEquals(char) {
  const code = StringPrototypeCharCodeAt(char, 0)
  return code === SEMICOLON_CHARCODE || code === EQUALS_CHARCODE;
}

function isQuoteOrBackslash(char) {
  const code = StringPrototypeCharCodeAt(char, 0)
  return code === QUOTE_CHARCODE || code === BACKSLASH_CHARCODE;
}

function isNotWhiteSpace(char) {
  return !RegExpPrototypeTest(WHITESPACE_REGEX, char);
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

  while (predicate(char) === false && index < length) {
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
      asCharCode = controlCharCode != null ? controlCharCode : asCharCode || StringPrototypeCharCodeAt(char, 0)
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
    const code = StringPrototypeCharCodeAt(char, 0)
    result += (code === QUOTE_CHARCODE || char === BACKSLASH_CHARCODE) ? `\\${char}` : char;
  }
  return result;
}

const encode = (value) => {
  if (value.length === 0) return '""';
  const isValid = RegExpPrototypeTest(NOT_HTTP_TOKEN_CODE_POINT, value);
  if (!isValid) return value;
  const escaped = escapeQuoteOrSolidus(value);
  return `"${escaped}"`;
};

class MIMEParams {
  #data = new SafeMap();

  delete(name) {
    this.#data.delete(name);
  }

  get(name) {
    return this.#data.get(name) ?? null;
  }

  has(name) {
    return this.#data.has(name);
  }

  set(name, value) {
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
    yield* this.#data.entries();
  }

  *keys() {
    yield* this.#data.keys();
  }

  *values() {
    yield* this.#data.values();
  }

  toString() {
    let ret = '';
    for (const { 0: key, 1: value } of this.#data) {
      const encoded = encode(value);
      // Ensure they are separated
      if (ret.length > 0) ret += ';';
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
    while (marker.position < endOfSource) {
      // Skip any whitespace before parameter
      moveUntil(str, isNotWhiteSpace, marker);
      const parameterName = moveUntil(str, isSemicolonOrEquals, marker, { toASCII: true });
      // If we are at end of the string, it cannot have a value
      if (marker.position > endOfSource) break;
      // Ignore parameters without values
      else if (StringPrototypeCharCodeAt(str, marker.position) === SEMICOLON_CHARCODE) {
        continue;
      } else {
        // Skip the terminating character
        marker.position++;
      }

      // Safe to use because we never do special actions for surrogate pairs
      const nextChar = StringPrototypeCharCodeAt(str, marker.position);
      let parameterValue = '';
      if (nextChar === QUOTE_CHARCODE) {
        // Handle quoted-string form of values
        // skip '"'
        marker.position++;
        // Find matching closing '"' or end of string
        // Unescape '\' quoted characters
        parameterValue = moveUntil(str, isQuoteOrBackslash, marker, { removeBackslashes: true });
        // If we did have an unmatched '\' add it back to the end
        if (StringPrototypeCharCodeAt(parameterValue, marker.position) === '\\') parameterValue += '\\';
      } else {
        // Handle the normal parameter value form
        const valueEnd = StringPrototypeIndexOf(str, SEMICOLON, marker.position);
        const rawValue = valueEnd === -1 ?
          StringPrototypeSlice(str, marker.position) :
          StringPrototypeSlice(str, marker.position, valueEnd);

        const trimmedValue = StringPrototypeTrimEnd(rawValue);
        marker.position += trimmedValue.length;

        // Ignore parameters without values
        if (trimmedValue.length === 0) continue;
        parameterValue = trimmedValue;
      }

      if (
        parameterName.length !== 0 &&
        RegExpPrototypeTest(
          NOT_HTTP_TOKEN_CODE_POINT,
          parameterName,
        ) === false &&
        RegExpPrototypeTest(
          NOT_HTTP_QUOTED_STRING_CODE_POINT,
          parameterValue,
        ) === false &&
        params.has(parameterName) === false
      ) {
        paramsMap.set(parameterName, parameterValue);
      }

      marker.position++;
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

const { parseParametersString } = MIMEParams;
delete MIMEParams.parseParametersString;

class MIMEType {
  #type;
  #subtype;
  #parameters;
  #essence;
  constructor(string) {
    string = `${string}`;
    const { type, subtype, parametersStringIndex } = parseTypeAndSubtype(string);
    this.#type = type;
    this.#subtype = subtype;
    this.#parameters = new MIMEParams();
    this.#essence = `${this.#type}/${this.#subtype}`;
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
