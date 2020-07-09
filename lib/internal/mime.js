'use strict';

const {
  ObjectCreate,
  MapIteratorPrototypeNext,
  RegExpPrototypeExec,
  RegExpPrototypeSymbolMatch,
  RegExpPrototypeSymbolMatchAll,
  RegExpPrototypeSymbolReplace,
  RegExpPrototypeSymbolSearch,
  RegExpPrototypeSymbolSplit,
  StringPrototypeReplace,
  StringPrototypeToLowerCase,
  StringPrototypeSlice,
  StringPrototypeCharAt,
  StringPrototypeIndexOf,
  StringPrototypeSearch,
  SymbolIterator,
  SymbolMatch,
  SymbolMatchAll,
  SymbolReplace,
  SymbolSearch,
  SymbolSplit,
  SafeMap: Map,
} = primordials;
const {
  ERR_INVALID_MIME_SYNTAX
} = require('internal/errors').codes;

function hardenRegExp(pattern) {
  pattern[SymbolMatch] = RegExpPrototypeSymbolMatch;
  pattern[SymbolMatchAll] = RegExpPrototypeSymbolMatchAll;
  pattern[SymbolReplace] = RegExpPrototypeSymbolReplace;
  pattern[SymbolSearch] = RegExpPrototypeSymbolSearch;
  pattern[SymbolSplit] = RegExpPrototypeSymbolSplit;
  return pattern;
}

const NotHTTPTokenCodePoint = hardenRegExp(/[^!#$%&'*+\-.^_`|~A-Za-z0-9]/g);
const NotHTTPQuotedStringCodePoint = hardenRegExp(/[^\t\u0020-~\u0080-\u00FF]/g);

const END_BEGINNING_WHITESPACE = hardenRegExp(/[^\r\n\t ]|$/);
const START_ENDING_WHITESPACE = hardenRegExp(/[\r\n\t ]*$/);

const ASCII_LOWER = hardenRegExp(/[A-Z]/g);
function toASCIILower(str) {
  return StringPrototypeReplace(str, ASCII_LOWER,
                                (c) => StringPrototypeToLowerCase(c));
}

const SOLIDUS = '/';
const SEMICOLON = ';';
function parseTypeAndSubtype(str) {
  // Skip only HTTP whitespace from start
  let position = StringPrototypeSearch(str, END_BEGINNING_WHITESPACE);
  // read until '/'
  const typeEnd = StringPrototypeIndexOf(str, SOLIDUS, position);
  const trimmedType = typeEnd === -1 ?
    StringPrototypeSlice(str, position) :
    StringPrototypeSlice(str, position, typeEnd);
  const invalidTypeIndex = StringPrototypeSearch(trimmedType,
                                                 NotHTTPTokenCodePoint);
  if (trimmedType === '' || invalidTypeIndex !== -1 || typeEnd === -1) {
    throw new ERR_INVALID_MIME_SYNTAX('type', str, invalidTypeIndex);
  }
  // skip type and '/'
  position = typeEnd + 1;
  const type = toASCIILower(trimmedType);
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
  const trimmedSubtype = StringPrototypeSlice(
    rawSubtype,
    0,
    StringPrototypeSearch(rawSubtype, START_ENDING_WHITESPACE));
  const invalidSubtypeIndex = StringPrototypeSearch(trimmedSubtype,
                                                    NotHTTPTokenCodePoint);
  if (trimmedSubtype === '' ||
    invalidSubtypeIndex !== -1) {
    throw new ERR_INVALID_MIME_SYNTAX('subtype', str, trimmedSubtype);
  }
  const subtype = toASCIILower(trimmedSubtype);
  const target = ObjectCreate(null);
  target.type = type;
  target.subtype = subtype;
  target.parametersStringIndex = position;
  return target;
}

const EQUALS_SEMICOLON_OR_END = hardenRegExp(/[;=]|$/);
const QUOTED_VALUE_PATTERN = hardenRegExp(/^(?:([\\]$)|[\\][\s\S]|[^"])*(?:(")|$)/u);
const QUOTED_CHARACTER = hardenRegExp(/[\\]([\s\S])/ug);
function parseParametersString(str, position, paramsMap = new Map()) {
  const endOfSource = StringPrototypeSearch(
    StringPrototypeSlice(str, position),
    START_ENDING_WHITESPACE
  ) + position;
  while (position < endOfSource) {
    // Skip any whitespace before parameter
    position += StringPrototypeSearch(
      StringPrototypeSlice(str, position),
      END_BEGINNING_WHITESPACE
    );
    // Read until ';' or '='
    const afterParameterName = StringPrototypeSearch(
      StringPrototypeSlice(str, position),
      EQUALS_SEMICOLON_OR_END
    ) + position;
    const parameterString = toASCIILower(
      StringPrototypeSlice(str, position, afterParameterName)
    );
    position = afterParameterName;
    // If we found a terminating character
    if (position < endOfSource) {
      // Safe to use because we never do special actions for surrogate pairs
      const char = StringPrototypeCharAt(str, position);
      // Skip the terminating character
      position += 1;
      // Ignore parameters without values
      if (char === ';') {
        continue;
      }
    }
    // If we are at end of the string, it cannot have a value
    if (position >= endOfSource) break;
    // Safe to use because we never do special actions for surrogate pairs
    const char = StringPrototypeCharAt(str, position);
    let parameterValue = null;
    if (char === '"') {
      // Handle quoted-string form of values
      // skip '"'
      position += 1;
      // Find matching closing '"' or end of string
      //   use $1 to see if we terminated on unmatched '\'
      //   use $2 to see if we terminated on a matching '"'
      //   so we can skip the last char in either case
      const insideMatch = RegExpPrototypeExec(
        QUOTED_VALUE_PATTERN,
        StringPrototypeSlice(str, position));
      position += insideMatch[0].length;
      // Skip including last character if an unmatched '\' or '"' during
      // unescape
      const inside = insideMatch[1] || insideMatch[2] ?
        StringPrototypeSlice(insideMatch[0], 0, -1) :
        insideMatch[0];
      // Unescape '\' quoted characters
      parameterValue = StringPrototypeReplace(inside, QUOTED_CHARACTER, '$1');
      // If we did have an unmatched '\' add it back to the end
      if (insideMatch[1]) parameterValue += '\\';
    } else {
      // Handle the normal parameter value form
      const valueEnd = StringPrototypeIndexOf(str, SEMICOLON, position);
      const rawValue = valueEnd === -1 ?
        StringPrototypeSlice(str, position) :
        StringPrototypeSlice(str, position, valueEnd);
      position += rawValue.length;
      const trimmedValue = StringPrototypeSlice(
        rawValue,
        0,
        StringPrototypeSearch(rawValue, START_ENDING_WHITESPACE));
      // Ignore parameters without values
      if (trimmedValue === '') continue;
      parameterValue = trimmedValue;
    }
    if (parameterString !== '' &&
      StringPrototypeSearch(parameterString,
                            NotHTTPTokenCodePoint) === -1 &&
      StringPrototypeSearch(parameterValue,
                            NotHTTPQuotedStringCodePoint) === -1 &&
      paramsMap.has(parameterString) === false) {
      paramsMap.set(parameterString, parameterValue);
    }
    position++;
  }
  return paramsMap;
}

const QUOTE_OR_SOLIDUS = hardenRegExp(/["\\]/g);
const encode = (value) => {
  if (value.length === 0) return '""';
  NotHTTPTokenCodePoint.lastIndex = 0;
  const encode = StringPrototypeSearch(value, NotHTTPTokenCodePoint) !== -1;
  if (!encode) return value;
  const escaped = StringPrototypeReplace(value, QUOTE_OR_SOLIDUS, '\\$&');
  return `"${escaped}"`;
};

const MIMEStringify = (type, subtype, parameters) => {
  let ret = `${type}/${subtype}`;
  const paramStr = MIMEParamsStringify(parameters);
  if (paramStr.length) ret += `;${paramStr}`;
  return ret;
};

const MIMEParamsStringify = (parameters) => {
  let ret = '';
  const entries = MIMEParamsData(parameters).entries();
  let keyValuePair, done;
  // Using this to avoid prototype pollution on Map iterators
  while ({ value: keyValuePair, done } =
        MapIteratorPrototypeNext(entries)) {
    if (done) break;
    const [key, value] = keyValuePair;
    const encoded = encode(value);
    // Ensure they are separated
    if (ret.length) ret += ';';
    ret += `${key}=${encoded}`;
  }
  return ret;
};

class MIMEParams {
  #data;
  constructor() {
    this.#data = new Map();
  }

  delete(name) {
    this.#data.delete(name);
  }

  get(name) {
    const data = this.#data;
    if (data.has(name)) {
      return data.get(name);
    }
    return null;
  }

  has(name) {
    return this.#data.has(name);
  }

  set(name, value) {
    const data = this.#data;
    NotHTTPTokenCodePoint.lastIndex = 0;
    name = `${name}`;
    value = `${value}`;
    const invalidNameIndex = StringPrototypeSearch(name, NotHTTPTokenCodePoint);
    if (name.length === 0 || invalidNameIndex !== -1) {
      throw new ERR_INVALID_MIME_SYNTAX('parameter name',
                                        name,
                                        invalidNameIndex);
    }
    NotHTTPQuotedStringCodePoint.lastIndex = 0;
    const invalidValueIndex = StringPrototypeSearch(
      value,
      NotHTTPQuotedStringCodePoint);
    if (invalidValueIndex !== -1) {
      throw new ERR_INVALID_MIME_SYNTAX('parameter value',
                                        value,
                                        invalidValueIndex);
    }
    data.set(name, value);
  }

  *entries() {
    return yield* this.#data.entries();
  }

  *keys() {
    return yield* this.#data.keys();
  }

  *values() {
    return yield* this.#data.values();
  }

  *[SymbolIterator]() {
    return yield* this.#data.entries();
  }

  toJSON() {
    return MIMEParamsStringify(this);
  }

  toString() {
    return MIMEParamsStringify(this);
  }

  // Used to act as a friendly class to stringifying stuff
  // not meant to be exposed to users, could inject invalid values
  static _data(o) {
    return o.#data;
  }
}

const MIMEParamsData = MIMEParams._data;
delete MIMEParams._data;

class MIMEType {
  #type;
  #subtype;
  #parameters;
  constructor(string) {
    string = `${string}`;
    const data = parseTypeAndSubtype(string);
    this.#type = data.type;
    this.#subtype = data.subtype;
    this.#parameters = new MIMEParams();
    parseParametersString(
      string,
      data.parametersStringIndex,
      MIMEParamsData(this.#parameters)
    );
  }

  get type() {
    return this.#type;
  }

  set type(v) {
    v = `${v}`;
    NotHTTPTokenCodePoint;
    const invalidTypeIndex = StringPrototypeSearch(v, NotHTTPTokenCodePoint);
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
    const invalidSubtypeIndex = StringPrototypeSearch(v, NotHTTPTokenCodePoint);
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

  toJSON() {
    return MIMEStringify(this.#type, this.#subtype, this.#parameters);
  }

  toString() {
    return MIMEStringify(this.#type, this.#subtype, this.#parameters);
  }
}
module.exports = {
  MIMEType,
  MIMEParams
};
