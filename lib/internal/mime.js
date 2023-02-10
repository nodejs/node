'use strict';
const {
  FunctionPrototypeCall,
  ObjectDefineProperty,
  RegExpPrototypeExec,
  SafeMap,
  SafeStringPrototypeSearch,
  StringPrototypeCharAt,
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

const NOT_HTTP_TOKEN_CODE_POINT = /[^!#$%&'*+\-.^_`|~A-Za-z0-9]/g;
const NOT_HTTP_QUOTED_STRING_CODE_POINT = /[^\t\u0020-~\u0080-\u00FF]/g;

const END_BEGINNING_WHITESPACE = /[^\r\n\t ]|$/;
const START_ENDING_WHITESPACE = /[\r\n\t ]*$/;
const WHITESPACE_REGEX = /[\r\n\t ]/;
const ASCII_LOWER_LOWERBOUND = StringPrototypeCharCodeAt('A');
const ASCII_LOWER_UPPERBOUND = StringPrototypeCharCodeAt('Z');

// TODO: evaluate later
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
function parseTypeAndSubtype(str) {
  // Skip only HTTP whitespace from start
  str = StringPrototypeTrim(str);
  // ### CURRENT
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
  // const trimmedSubtype = StringPrototypeSlice(
  //   rawSubtype,
  //   0,
  //   SafeStringPrototypeSearch(rawSubtype, START_ENDING_WHITESPACE));
  const trimmedSubtype = StringPrototypeTrimEnd(rawSubtype);
  // const invalidSubtypeIndex = SafeStringPrototypeSearch(trimmedSubtype,
  //                                                       NOT_HTTP_TOKEN_CODE_POINT);
  if (trimmedSubtype === '' || RegExpPrototypeTest(NOT_HTTP_TOKEN_CODE_POINT,
    trimmedSubtype)) {
    throw new ERR_INVALID_MIME_SYNTAX('subtype', str, trimmedSubtype);
  }
  const subtype = toASCIILower(trimmedSubtype);
  // // ### NEW
  // const marker = { position: 0 };
  // // read until '/'
  // const type = moveUntilIterative(str, isSolidus, marker, true);
  // if (type === '' || RegExpPrototypeTest( NOT_HTTP_TOKEN_CODE_POINT,
  //   type)) {
  //   throw new ERR_INVALID_MIME_SYNTAX('type', str, invalidTypeIndex);
  // }
  // // skip type and '/'
  // marker.position += 1;

  // // read until ';'
  // const subtype = moveUntilIterative(str, isSemicolon, marker, true)
  // const trimmedSubtype = StringPrototypeTrimEnd(subtype);
  // if (trimmedSubtype === '' || RegExpPrototypeTest(NOT_HTTP_TOKEN_CODE_POINT,
  //   trimmedSubtype)) {
  //   throw new ERR_INVALID_MIME_SYNTAX('subtype', str, trimmedSubtype);
  // }

  // marker.position += 1;
  
  return {
    __proto__: null,
    type,
    subtype,
    parametersStringIndex: position,
    // parametersStringIndex: marker.position,
  };
}

const EQUALS_SEMICOLON_OR_END = /[;=]|$/;
const QUOTED_VALUE_PATTERN = /^(?:([\\]$)|[\\][\s\S]|[^"])*(?:(")|$)/u;

// TESTING AREA
function isSemicolonOrEquals(char) {
  return char === SEMICOLON || char === '=';
}

function isQuoteOrBackslash(char) {
  return char === '"' || char === '\\';
}

function isNotWhiteSpace(char) {
  return !WHITESPACE_REGEX.test(char);
}

function moveUntilIterative(str, predicate, marker, { toASCII, removeBackslashes } = { toASCII: false, removeBackslashes: false }) {
  const start = marker.position;
  const length = str.length;
  let index = start;
  let result = '';

  while(predicate(str[index]) === false && index < length) {
    let char = str[index];
    if (removeBackslashes && char === '\\') {
      // jump to the next char
      index++
      char = str[index];
    }

    if (toASCII) {
      const asCharCode = StringPrototypeCharCodeAt(char)
      result += asCharCode >= ASCII_LOWER_LOWERBOUND && asCharCode <= ASCII_LOWER_UPPERBOUND ?
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

// END-TESTING AREA

// function removeBackslashes(str) {
//   let ret = '';
//   // We stop at str.length - 1 because we want to look ahead one character.
//   let i;
//   for (i = 0; i < str.length - 1; i++) {
//     const c = str[i];
//     if (c === '\\') {
//       i++;
//       ret += str[i];
//     } else {
//       ret += c;
//     }
//   }
//   // We add the last character if we didn't skip to it.
//   if (i === str.length - 1) {
//     ret += str[i];
//   }
//   return ret;
// }


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
  if (isValid) return value;
  const escaped = escapeQuoteOrSolidus(value);
  return `"${escaped}"`;
};

class MIMEParams {
  #data = new SafeMap();

  delete(name) {
    this.#data.delete(name);
  }

  get(name) {
    const data = this.#data;
    return data.get(name) ?? null;
  }

  has(name) {
    return this.#data.has(name);
  }

  set(name, value) {
    const data = this.#data;
    name = `${name}`;
    value = `${value}`;
    const invalidNameIndex = RegExpPrototypeTest(NOT_HTTP_TOKEN_CODE_POINT, name);
    if (name.length === 0 || invalidNameIndex) {
      throw new ERR_INVALID_MIME_SYNTAX(
        'parameter name',
        name,
        invalidNameIndex,
      );
    }
    const invalidValueIndex = RegExpPrototypeTest(
      NOT_HTTP_QUOTED_STRING_CODE_POINT,
      value);
    if (invalidValueIndex) {
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
      if (ret.length) ret += ';';
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
      moveUntilIterative(str, isNotWhiteSpace, marker)
      const parameterName = moveUntilIterative(str, isSemicolonOrEquals, marker, { toASCII: true });
      // If we are at end of the string, it cannot have a value
      if (marker.position > endOfSource) break;
      else {
        // Ignore parameters without values
        if (str[marker.position] === ';') {
          continue;
        } else {
          // Skip the terminating character
          marker.position++;
        }
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
        parameterValue = moveUntilIterative(str, isQuoteOrBackslash, marker, { removeBackslashes: true });
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
        params.has(parameterName) === false &&
        RegExpPrototypeTest(NOT_HTTP_TOKEN_CODE_POINT,
          parameterName) === false &&
        RegExpPrototypeTest(NOT_HTTP_QUOTED_STRING_CODE_POINT,
          parameterName) === false
      ) {
        paramsMap.set(parameterName, parameterValue);
      }

      marker.position++;
    }
    return paramsMap;
  }
  // static parseParametersString(str, position, params) {
  //   const paramsMap = params.#data;
  //   const endOfSource = SafeStringPrototypeSearch(
  //     StringPrototypeSlice(str, position),
  //     START_ENDING_WHITESPACE
  //   ) + position;
  //   while (position < endOfSource) {
  //     // Skip any whitespace before parameter
  //     position += SafeStringPrototypeSearch(
  //       StringPrototypeSlice(str, position),
  //       END_BEGINNING_WHITESPACE
  //     );
  //     // Read until ';' or '='
  //     const afterParameterName = SafeStringPrototypeSearch(
  //       StringPrototypeSlice(str, position),
  //       EQUALS_SEMICOLON_OR_END
  //     ) + position;
  //     const parameterString = toASCIILower(
  //       StringPrototypeSlice(str, position, afterParameterName)
  //     );
  //     position = afterParameterName;
  //     // If we found a terminating character
  //     if (position < endOfSource) {
  //       // Safe to use because we never do special actions for surrogate pairs
  //       const char = StringPrototypeCharAt(str, position);
  //       // Skip the terminating character
  //       position += 1;
  //       // Ignore parameters without values
  //       if (char === ';') {
  //         continue;
  //       }
  //     }
  //     // If we are at end of the string, it cannot have a value
  //     if (position >= endOfSource) break;
  //     // Safe to use because we never do special actions for surrogate pairs
  //     const char = StringPrototypeCharAt(str, position);
  //     let parameterValue = null;
  //     if (char === '"') {
  //       // Handle quoted-string form of values
  //       // skip '"'
  //       position += 1;
  //       // Find matching closing '"' or end of string
  //       //   use $1 to see if we terminated on unmatched '\'
  //       //   use $2 to see if we terminated on a matching '"'
  //       //   so we can skip the last char in either case
  //       const insideMatch = RegExpPrototypeExec(
  //         QUOTED_VALUE_PATTERN,
  //         StringPrototypeSlice(str, position));
  //       position += insideMatch[0].length;
  //       // Skip including last character if an unmatched '\' or '"' during
  //       // unescape
  //       const inside = insideMatch[1] || insideMatch[2] ?
  //         StringPrototypeSlice(insideMatch[0], 0, -1) :
  //         insideMatch[0];
  //       // Unescape '\' quoted characters
  //       parameterValue = removeBackslashes(inside);
  //       // If we did have an unmatched '\' add it back to the end
  //       if (insideMatch[1]) parameterValue += '\\';
  //     } else {
  //       // Handle the normal parameter value form
  //       const valueEnd = StringPrototypeIndexOf(str, SEMICOLON, position);
  //       const rawValue = valueEnd === -1 ?
  //         StringPrototypeSlice(str, position) :
  //         StringPrototypeSlice(str, position, valueEnd);
  //       position += rawValue.length;
  //       const trimmedValue = StringPrototypeSlice(
  //         rawValue,
  //         0,
  //         SafeStringPrototypeSearch(rawValue, START_ENDING_WHITESPACE)
  //       );
  //       // Ignore parameters without values
  //       if (trimmedValue === '') continue;
  //       parameterValue = trimmedValue;
  //     }
  //     if (
  //       parameterString !== '' &&
  //       RegExpPrototypeTest(NOT_HTTP_TOKEN_CODE_POINT,
  //         parameterString) === false &&
  //       RegExpPrototypeExec(NOT_HTTP_QUOTED_STRING_CODE_POINT,
  //         parameterString) === false &&
  //       params.has(parameterString) === false
  //     ) {
  //       paramsMap.set(parameterString, parameterValue);
  //     }
  //     position++;
  //   }
  //   return paramsMap;
  // }
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
    const data = parseTypeAndSubtype(string);
    this.#type = data.type;
    this.#subtype = data.subtype;
    this.#parameters = new MIMEParams();
    this.#essence = `${this.#type}/${this.#subtype}`;
    parseParametersString(
      string,
      data.parametersStringIndex,
      this.#parameters,
    );
  }

  get type() {
    return this.#type;
  }

  set type(v) {
    v = `${v}`;
    const invalidTypeIndex = RegExpPrototypeTest(NOT_HTTP_TOKEN_CODE_POINT, v);
    if (v.length === 0 || invalidTypeIndex) {
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
    const invalidSubtypeIndex = RegExpPrototypeTest(NOT_HTTP_TOKEN_CODE_POINT, v);
    if (v.length === 0 || invalidSubtypeIndex) {
      throw new ERR_INVALID_MIME_SYNTAX('subtype', v, invalidSubtypeIndex);
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
