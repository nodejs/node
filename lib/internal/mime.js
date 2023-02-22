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

function toASCIILower(str) {
  let result = '';
  for (let i = 0; i < str.length; i++) {
    const char = str[i];

    result += char >= 'A' && char <= 'Z' ?
      StringPrototypeToLowerCase(char) :
      char;
  }
  return result;
}

const SOLIDUS = '/';
const SEMICOLON = ';';
const EQUALS = '=';
const BACKSLASH = '\\';
const QUOTE = '"';
function parseTypeAndSubtype(str) {
  // Skip only HTTP whitespace from start
  str = StringPrototypeTrim(str);
  let position = 0;
  // read until '/'
  const typeEnd = StringPrototypeIndexOf(str, SOLIDUS);
  const mimetype = typeEnd === -1 ?
    StringPrototypeSlice(str, position) :
    StringPrototypeSlice(str, position, typeEnd);
  if (mimetype === '' || typeEnd === -1 || RegExpPrototypeTest(
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
    StringPrototypeSlice(str, position) :
    StringPrototypeSlice(str, position, subtypeEnd);
  position += rawSubtype.length;
  if (subtypeEnd !== -1) {
    // skip ';'
    position += 1;
  }
  const trimmedSubtype = StringPrototypeTrimEnd(rawSubtype);
  if (trimmedSubtype === '' ||
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
  return char === SEMICOLON || char === EQUALS;
}

function isQuoteOrBackslash(char) {
  return char === QUOTE || char === BACKSLASH;
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

  while (predicate(str[index]) === false && index < length) {
    let char = str[index];
    if (removeBackslashes && char === '\\') {
      // Jump to the next char
      index++;
      char = str[index];
    }

    if (toASCII) {
      const asCharCode = StringPrototypeCharCodeAt(char);
      result += asCharCode >= ASCII_LOWER_LOWERBOUND &&
        asCharCode <= ASCII_LOWER_UPPERBOUND ?
        StringPrototypeToLowerCase(char) :
        char;
    } else {
      result += char;
    }

    index++;
  }

  marker.position = index;

  return result;
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
      else if (str[marker.position] === ';') {
        continue;
      } else {
        // Skip the terminating character
        marker.position++;
      }

      // Safe to use because we never do special actions for surrogate pairs
      const nextChar = str[marker.position];
      let parameterValue = null;
      if (nextChar === '"') {
        // Handle quoted-string form of values
        // skip '"'
        marker.position++;
        // Find matching closing '"' or end of string
        // Unescape '\' quoted characters
        parameterValue = moveUntil(str, isQuoteOrBackslash, marker, { removeBackslashes: true });
        // If we did have an unmatched '\' add it back to the end
        if (parameterValue[marker.position] === '\\') parameterValue += '\\';
      } else {
        // Handle the normal parameter value form
        const valueEnd = StringPrototypeIndexOf(str, SEMICOLON, marker.position);
        const rawValue = valueEnd === -1 ?
          StringPrototypeSlice(str, marker.position) :
          StringPrototypeSlice(str, marker.position, valueEnd);

        const trimmedValue = StringPrototypeTrimEnd(rawValue);
        marker.position += trimmedValue.length;

        // Ignore parameters without values
        if (trimmedValue === '') continue;
        parameterValue = trimmedValue;
      }

      if (
        parameterName !== '' &&
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
    if (v.length === 0 || invalidTypeIndex !== -1) {
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
    if (v.length === 0 || invalidTypeIndex !== -1) {
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
