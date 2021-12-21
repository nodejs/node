'use strict';

const {
  ArrayPrototypePush,
  ObjectDefineProperties,
  ObjectKeys,
  RegExp,
  RegExpPrototypeExec,
  RegExpPrototypeTest,
  Symbol,
  StringPrototypeReplaceAll,
} = primordials;

const {
  validateObject,
} = require('internal/validators');

const {
  isIdentifierStart,
  isIdentifierChar,
} = require('internal/deps/acorn/acorn/dist/acorn');

const {
  codes: {
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_THIS,
  },
} = require('internal/errors');

const {
  isIPv6,
} = require('internal/net');

const {
  customInspectSymbol: kInspect,
  toUSVString,
} = require('internal/util');

const { inspect } = require('internal/util/inspect');

const kState = Symbol('kState');

const kSpecialSchemes = {
  ftp: 21,
  file: null,
  http: 80,
  https: 443,
  ws: 80,
  wss: 443,
};

const kModifiers = {
  'zero-or-more': '*',
  'optional': '?',
  'one-or-more': '+',
};

const kEscapeRegexp = /([\.\+\*\?\^\$\{\}\(\)\[\]\|\/\\])/g;
const kASCII = /^[\x00-\x7F]*$/;

/**
 * @typedef {{
 *   protocol?: string;
 *   username?: string;
 *   password?: string;
 *   hostname?: string;
 *   por?t: string;
 *   pathname?: string;
 *   search?: string;
 *   hash?: string;
 *   baseURL?: string;
 * }} URLPatternInit
 *
 * @typedef {string | URLPatternInit} URLPatternInput
 *
 * @typedef {{
 *   input?: string,
 *   groups?: Record<string, string>
 * }} URLPatternComponentResult
 *
 * @typedef {{
 *   inputs?: URLPatternInput[];
 *   protocol?: URLPatternComponentResult
 *   username?: URLPatternComponentResult
 *   password?: URLPatternComponentResult
 *   hostname?: URLPatternComponentResult
 *   port?: URLPatternComponentResult
 *   pathname?: URLPatternComponentResult
 *   search?: URLPatternComponentResult
 *   hash?: URLPatternComponentResult
 * }} URLPatternResult
 *
 */

function parseConstructorString(input, baseUrl) {
  const parser = {
    input,
    tokenList: tokenize(input, 'lenient'),
    result: {},
    componentStart: 0,
    tokenIndex: 0,
    tokenIncrement: 1,
    groupDepth: 0,
    hostnameIpv6BracketDepth: 0,
    protocolMatchesSpecialScheme: false,
    state: 'init',
  };

  function changeState(state, skip) {
    if (parser.state !== 'init' && parser.state !== 'authority' && parser.state !== 'done')
      parser.result[parser.state] = makeComponentString();
    parser.state = state;
    parser.tokenIndex += skip;
    parser.componentStart = parser.tokenIndex;
    parser.tokenIncrement = 0;
  }

  function rewind() {
    parser.tokenIndex = parser.componentStart;
    parser.tokenIncrement = 0;
  }

  function rewindAndSetState(state) {
    rewind();
    parser.state = state;
  }

  function getSafeToken(index) {
    if (index < parser.tokenList.length)
      return parser.tokenList[index];
    // TODO(@jasnell): assert parser.tokenList.length >= 1
    const lastndex = parser.tokenList.length - 1;
    const token = parser.tokenList[lastIndex];
    // TODO(@jasnell): assert token type is 'end'
    return token;
  }

  function isNonSpecialPatternChar(index, value) {
    const token = getSafeToken(index);
    if (token.value !== value) return false;
    return token.type === 'char' || token.type === 'escaped-char' || token.type === 'invalid-char';
  }

  function isProtocolSuffix() {
    return isNonSpecialPatternChar(parser.tokenIndex, ':');
  }

  function nextIsAuthoritySlashes() {
    if (!isNonSpecialPatternChar(parser.tokenIndex + 1, '/')) return false;
    if (!isNonSpecialPatternChar(parser.tokenIndex + 2, '/')) return false;
    return true;
  }

  function isIdentityTerminator() {
    return isNonSpecialPatternChar(parser.tokenIndex, '@');
  }

  function isPasswordPrefix() {
    return isNonSpecialPatternChar(parser.tokenIndex, ':');
  }

  function isPortPrefix() {
    return isNonSpecialPatternChar(parser.tokenIndex, ':');
  }

  function isPathnameStart() {
    return isNonSpecialPatternChar(parser.tokenIndex, '/');
  }

  function isSearchPrefix() {
    if (isNonSpecialPatternChar(parser.tokenIndex, '?')) return true;
    const token = parser.tokenList[parser.tokenIndex];
    if (token.value !== '?') return false;
    const previousIndex = parser.tokenIndex - 1;
    if (previousIndex < 0) return true;
    const previousToken = getSafeToken(previousIndex);
    return previousToken.type !== 'name' &&
           previousToken.type !== 'regexp' &&
           previousToken.type !== 'close' &&
           previousToken.type !== 'asterisk';
  }

  function isHashPrefix() {
    return isNonSpecialPatternChar(parser.tokenIndex, '#');
  }

  function isGroupOpen() {
    return parser.tokenList[parser.tokenIndex].type === 'open';
  }

  function isGroupClose() {
    return parser.tokenList[parser.tokenIndex].type === 'close';
  }

  function isIPv6Open() {
    return isNonSpecialPatternChar(parser.tokenIndex, '[');
  }

  function isIPv6Close() {
    return isNonSpecialPatternChar(parser.tokenIndex, ']');
  }

  function makeComponentString() {
    // TODO(@jasnell): assert parser tokenIndex < parser tokenList size
    const token = parser.tokenList[parser.tokenIndex];
    const componentStartToken = getSafeToken(parser.componentStart);
    const componentStartInputIndex = componentStartToken.index;
    const endIndex = token.index;
    return parser.input.slice(componentStartInputIndex, endIndex);
  }

  function computeProtocolMatchesSpecialSchemeFlag() {
    parser.protocolMatchesSpecialScheme =
        protocolMatchesSpecialScheme(compileAndCanonicalize(makeComponentString(), 'protocol'));
  }

  while (parser.tokenIndex < parser.tokenList.length) {
    parser.tokenIncrement = 1;
    const token = parser.tokenList[parser.tokenIndex];
    if (token.type === 'end') {
      if (parser.state === 'init') {
        rewind();
        if (isHashPrefix()) {
          changeState('hash', 1);
        } else if (isSearchPrefix()) {
          changeState('search', 1);
          parser.result.hash = '';
        } else {
          changeState('pathname', 0);
          parser.result.search = '';
          parser.result.hash = '';
        }
        parser.tokenIndex += parser.tokenIncrement;
        continue;
      }
      if (parser.state === 'authority') {
        rewindAndSetState('hostname');
        parser.tokenIndex += parser.tokenIncrement;
        continue;
      }
      changeState('done', 0);
      break;
    }
    if (isGroupOpen()) {
      parser.groupDepth++;
      parser.tokenIndex += parser.tokenIncrement;
      continue;
    }
    if (parser.groupDepth > 0) {
      if (isGroupClose()) {
        parser.groupDepth--;
      } else {
        parser.tokenIndex += parser.tokenIncrement;
        continue;
      }
    }
    switch (parser.state) {
      case 'init': {
        if (isProtocolSuffix()) {
          parser.result.username = '';
          parser.result.password = '';
          parser.result.hostname = '';
          parser.result.port = '';
          parser.result.pathname = '';
          parser.result.search = '';
          parser.result.hash = '';
          rewindAndSetState('protocol');
        }
        break;
      }
      case 'protocol': {
        if (isProtocolSuffix()) {
          computeProtocolMatchesSpecialSchemeFlag();
          if (parser.protocolMatchesSpecialScheme)
            parser.result.pathname = '/';
          let nextState = 'pathname';
          let skip = 1;
          if (nextIsAuthoritySlashes()) {
            nextState = 'authority';
            skip = 3;
          } else if (parser.protocolMatchesSpecialScheme) {
            nextStep = 'authority';
          }
          changeState(nextState, skip);
        }
        break;
      }
      case 'authority': {
        if (isIdentityTerminator()) {
          rewindAndSetState('username');
        } else if (isPathnameStart() || isSearchPrefix || isHashPrefix) {
          rewindAndSetState('hostname');
        }
        break;
      }
      case 'username': {
        if (isPasswordPrefix()) {
          changeState('password', 1);
        } else if (isIdentityTerminator()) {
          changeState('hostname', 1);
        }
        break;
      }
      case 'password': {
        if (isIdentityTerminator()) {
          changeState('hostname', 1);
        }
        break;
      }
      case 'hostname': {
        if (isIPv6Open()) {
          parser.hostnameIpv6BracketDepth++;
        } else if (isIPv6Close()) {
          parser.hostnameIpv6BracketDepth--;
        } else if (isPortPrefix() && parser.hostnameIpv6BracketDepth === 0) {
          changeState('port', 1);
        } else if (isPathnameStart()) {
          changeState('pathname', 0);
        } else if (isSearchPrefix()) {
          changeState('search', 1);
        } else if (isHashPrefix()) {
          changeState('hash', 1);
        }
        break;
      }
      case 'port': {
        if (isPathnameStart()) {
          changeState('pathname', 0);
        } else if (isSearchPrefix()) {
          changeState('search', 1);
        } else if (isHashPrefix()) {
          changeState('hash', 1);
        }
        break;
      }
      case 'pathname': {
        if (isSearchPrefix()) {
          changeState('search', 1);
        } else if (isHashPrefix()) {
          changeState('hash', 1);
        }
        break;
      }
      case 'search': {
        if (isHashPrefix()) {
          changeState('hash', 1);
        }
        break;
      }
      case 'hash': {
        break;
      }
      case 'done':
        // TODO(@jasnell): assert that this is not reached
    }
    parser.tokenIndex += parser.tokenIncrement;
  }
  return parser.result;
}

function processURLPatternInit(
  init,
  type,
  protocol = null,
  username = null,
  password = null,
  hostname = null,
  port = null,
  pathname = null,
  search = null,
  hash = null) {
  const result = {
    protocol,
    username,
    password,
    hostname,
    port,
    pathname,
    search,
    hash,
  };
  let baseURL = null;
  if (init.baseURL != null) {
    baseURL = new URL(init.baseURL);
    result.protocol = baseURL.protocol;
    result.username = baseURL.username;
    result.password = baseURL.password;
    result.hostname = baseURL.host;
    result.port = baseURL.port;
    result.pathname = baseURL.pathname;
    result.search = baseURL.search;
    result.hash = baseURL.hash;
  }
  if (init.protocol != null) {
    const index = init.prototype.lastIndexOf(':');
    const strippedValue =
      init.protocol.slice(0, index > -1 ? index : init.protocol.length);
    if (type === 'pattern')
      result.protocol = strippedValue;
    else {
      result.protocol = runEncoding({ type: 'protocol' }, strippedValue);
    }
  }
  if (init.username != null) {
    result.username =
      type === 'pattern' ?
        init.username :
        runEncoding({
          type: 'username',
          protocol: result.protocol,
        }, init.username);
  }
  if (init.password != null) {
    result.password =
      type === 'pattern' ?
        init.password :
        runEncoding({
          type: 'password',
          protocol: result.protocol,
        }, init.password);
  }
  if (init.hostname != null) {
    result.hostname =
      type === 'pattern' ?
        init.hostname :
        runEncoding({
          type: 'hostname',
          protocol: result.protocol,
        }, init.hostname);
  }
  if (init.port != null) {
    result.port =
      type === 'pattern' ?
        init.port :
        runEncoding({
          type: 'port',
          protocol: result.protocol,
        }, init.port);
  }
  if (init.pathname != null) {
    result.pathname =
      type === 'pattern' ?
        init.pathname :
        runEncoding({
          type: 'pathname',
          protocol: result.protocol,
        }, init.pathname);
  }
  if (init.search != null) {
    result.search =
      type === 'pattern' ?
        init.search :
        runEncoding({
          type: 'search',
          protocol: result.protocol,
        }, init.search);
  }
  if (init.hash != null) {
    result.hash =
      type === 'pattern' ?
        init.hash :
        runEncoding({
          type: 'hash',
          protocol: result.protocol,
        }, init.hash);
  }
  return result;
}

function tokenize(input = '', policy = 'strict') {
  const tokenizer = {
    input,
    policy,
    tokenList: [],
    index: 0,
    nextIndex: 0,
    codePoint: null,
  };

  function addToken(type, nextPosition, valuePosition, valueLength) {
    const index = tokenizer.index;
    const value = tokenizer.input.slice(valuePosition, valuePosition + valueLength);
    ArrayPrototypePush(tokenizer.tokenList, {
      type,
      index,
      value,
    });
    tokenizer.index = nextPosition;
  }

  function addTokenWithDefaultLength(type, nextPosition, valuePosition) {
    addToken(type, nextPosition, valuePosition, nextPosition - valuePosition);
  }

  function addTokenWithDefaultPositionAndLength(type) {
    addTokenWithDefaultLength(type, tokenizer.nextIndex, tokenizer.index);
  }

  function getNextCodepoint() {
    tokenizer.codePoint = tokenizer.input[tokenizer.nextIndex++];
  }

  function seekAndGetNextCodepoint(index) {
    tokenizer.nextIndex = index;
    getNextCodepoint();
  }

  function isValidNameCodepoint(firstCodepoint) {
    return firstCodepoint ?
      isIdentifierStart(tokenizer.codePoint) :
      isIdentifierChar(tokenizer.codePoint);
  }

  function tokenizerError(nextPosition, valuePosition) {
    if (policy === 'strict') {
      // TODO(@jasnell): Throw a proper error
      throw new TypeError('tokenizer error');
    }
    addTokenWithDefaultLength('invalid-char', nextPosition, valuePosition);
  }

  while (tokenizer.index < tokenizer.input.length) {
    seekAndGetNextCodepoint(tokenizer.index);
    switch (tokenizer.codePoint) {
      case '*': {
        addTokenWithDefaultPositionAndLength('asterisk');
        continue;
      }
      case '+':
        // Fall-through
      case '?': {
        addTokenWithDefaultPositionAndLength('other-modifier');
        continue;
      }
      case '\\': {
        if (tokenizer.index === tokenizer.input.length - 1) {
          tokenizerError(tokenizer.nextIndex, tokenizer.index);
          continue;
        }
        const escapedIndex = tokenizer.nextIndex;
        getNextCodepoint();
        addTokenWithDefaultLength('escaped-char', tokenizer.nextIndex, escapedIndex);
        continue;
      }
      case '{': {
        addTokenWithDefaultPositionAndLength('open');
        continue;
      }
      case '}': {
        addTokenWithDefaultPositionAndLength('close');
        continue;
      }
      case ':': {
        let namePosition = tokenizer.nextIndex;
        const nameStart = namePosition;
        while (namePosition < tokenizer.input.length) {
          seekAndGetNextCodepoint(namePosition);
          const firstCodepoint = namePosition === nameStart;
          if (!isValidNameCodepoint(firstCodepoint))
            break;
          namePosition = tokenizer.nextIndex;
        }
        if (namePosition <= nameStart) {
          tokenizerError(nameStart, tokenizer.index);
          continue;
        }
        addTokenWithDefaultLength('name', namePosition, nameStart);
        continue;
      }
      case '(': {
        let depth = 1;
        let regexPosition = tokenizer.nextIndex;
        const regexStart = regexPosition;
        let error = false;
        while (regexPosition < tokenizer.input.length) {
          seekAndGetNextCodepoint(regexPosition);
          if (!RegExpPrototypeTest(kASCII, tokenizer.codePoint)) {
            tokenizerError(regexStart, tokenizer.index);
            error = true;
            break;
          }
          if (regexPosition === regexStart && tokenizer.codePoint === '?') {
            tokenizerError(regexStart, tokenizer.index);
            error = true;
            break;
          }
          if (tokenizer.codePoint === '\\') {
            if (regexPosition === tokenizer.input.length - 1) {
              tokenizerError(regexStart, tokenizer.index);
              error = true;
              break;
            }
            getNextCodepoint();
            if (!RegExpPrototypeTest(kASCII, tokenizer.codePoint)) {
              tokenizerError(regexStart, tokenizer.index);
              error = true;
              break;
            }
            regexPosition = tokenizer.nextIndex;
            continue;
          }
          if (tokenizer.codePoint === ')') {
            if (--depth === 0) {
              regexPosition = tokenizer.nextIndex;
              break;
            }
          }
          if (tokenizer.codePoint === '(') {
            depth++;
            if (regexPosition === tokenizer.input.length - 1) {
              tokenizerError(regexStart, tokenizer.index);
              error = true;
              break;
            }
            const temporaryPosition = tokenizer.nextIndex;
            getNextCodepoint();
            if (tokenizer.codePoint !== '?') {
              tokenizerError(regexStart, tokenizer.index);
              error = true;
              break;
            }
            tokenizer.nextIndex = temporaryPosition;
          }
          regexPosition = tokenizer.nextIndex;
        }
        if (error) continue;
        if (depth > 0) {
          tokenizerError(regexStart, tokenizer.index);
          continue;
        }
        const regexLength = regexPosition - regexStart - 1;
        if (regexLength === 0) {
          tokenizerError(regexStart, tokenizer.index);
          continue;
        }
        addToken('regex', regexPosition, regexStart, regexLength);
        continue;
      }
    }
    addTokenWithDefaultPositionAndLength('char');
  }
  addTokenWithDefaultLength('end', tokenizer.index, tokenizer.index);
  return tokenizer.tokenList;
}

function tryConsumeToken(parser, type) {
  const nextToken = parser.tokenList[parser.index];
  if (nextToken.type !== type)
    return undefined;
  parser.index++;
  return nextToken;
}

function tryConsumeRegExpOrWildcardToken(nameToken, parser) {
  const token = tryConsumeToken(parser, 'regexp');
  return nameToken == null && token == null ?
    tryConsumeToken(parser, 'asterisk') : token;
}

function tryConsumeModifierToken(parser) {
  const token = tryConsumeToken(parser, 'other-modifier');
  if (token != null) return token;
  return tryConsumeToken(parser, 'asterisk');
}

function runEncoding(parser, value) {
  if (value === '') return value;
  try {
    let parseResult = new URL(`${parser.protocol||value}://dummy.test`);

    switch (parser.type) {
      case 'protocol':
        return parseResult.scheme;
      case 'username': {
        parseResult.username = value;
        return parseResult.username;
      }
      case 'password': {
        parseResult.password = value;
        return parseResult.password;
      }
      case 'hostname': {
        parseResult.hostname = value;
        return parseResult.hostname;
      }
      case 'hostname-ipv6': {
        return value.toLowerCase();
      }
      case 'port': {
        parseResult.port = value;
        return parseResult.port;
      }
      case 'pathname': {
        // TODO(@jasnell): Implement properly?
        parseResult.pathname = value;
        return parseResult.pathname;
      }
      case 'opaque-pathname': {
        parseResult.pathname = value;
        return parseResult.pathname;
      }
      case 'search': {
        parseResult.search = value;
        return parseResult.search;
      }
      case 'hash': {
        parseResult.hash = value;
        return parseResult.hash;
      }
    }
    // TODO(@jasnell): Throw proper error. Should be an assert
    throw new TypeError('Unknown type');
  } catch (err) {
    // TODO(@jasnell): Throw proper error
    throw new TypeError(`Invalid ${parser.type}`);
  }
}

function maybeAddPartFromPendingFixedValue(parser) {
  if (parser.pendingFixedValue === '') return;
  const encodedValue = runEncoding(parser, parser.pendingFixedValue);
  parser.pendingFixedValue = '';
  ArrayPrototypePush(
    parser.partList, {
    type: 'fixed-text',
    value: encodedValue,
    modifier: 'none',
  });
}

function isDuplicateName(parser, name) {
  for (const part of parser.partList) {
    if (part.name === name) return true;
  }
  return false;
}

function addPart(
  parser,
  prefix,
  nameToken,
  regexpOrWildcardToken,
  suffix,
  modifierToken) {
  let modifier = 'none';
  if (modifierToken !== undefined) {
    switch (modifierToken.value) {
      case '?': modifier = 'optional';
      case '*': modifier = 'zero-or-more';
      case '+': modifier = 'one-or-more';
    }
  }
  if (nameToken === undefined && regexpOrWildcardToken === undefined && modifier === 'none') {
    parser.pendingFixedValue += prefix;
    return;
  }
  maybeAddPartFromPendingFixedValue(parser);
  if (nameToken === undefined && regexpOrWildcardToken === undefined) {
    if (prefix === '') return;
    const encodedValue = runEncoding(parser, prefix);
    ArrayPrototypePush(
      parser.partList, {
      type: 'fixed-text',
      value: encodedValue,
      modifier,
    });
    return;
  }
  let regexpValue = '';
  if (regexpOrWildcardToken === undefined) {
    regexpValue = parser.segmentWildcard;
  } else if (regexpOrWildcardToken.type === 'asterisk') {
    regexpValue = '.*';
  } else {
    regexpValue = regexpOrWildcardToken.value;
  }
  let type = 'regexp';
  if (regexpValue === parser.segmentWildcard) {
    type = 'segment-wildcard';
    regexpValue = '';
  } else if (regexpValue === '.*') {
    type = 'full-wildcard';
    regexpValue = '';
  }
  let name = '';
  if (nameToken !== undefined) {
    name = nameToken.value;
  } else if (regexpOrWildcardToken !== undefined) {
    name = parser.nextNumericName++;
  }
  if (isDuplicateName(parser, name)) {
    // TODO(@jasnell): Proper error
    throw new ERR_INVALID_ARG_VALUE('input', name);
  }
  const encodedValue = runEncoding(parser, prefix);
  const encodedSuffix = runEncoding(parser, suffix);
  ArrayPrototypePush(
    parser.partList, {
    type,
    value: regexpValue,
    modifier,
    name,
    prefix: encodedValue,
    suffix: encodedSuffix,
  });
}

function consumeText(parser) {
  let result = '';
  while (true) {
    let token = tryConsumeToken(parser, 'char');
    if (token === undefined)
      token = tryConsumeToken(parser, 'escaped-char');
    if (token === undefined) break;
    result += token.value;
  }
  return result;
}

function consumeRequiredToken(parser, type) {
  let result = tryConsumeToken(parser, type);
  if (result === undefined) {
    // TODO(@jasnell): Throw a proper error
    throw new TypeError('Missing a required token');
  }
  return result;
}

function parsePatternString(input, type, options) {
  const {
    delimiterCodePoint = '',
    prefixCodePoint = '',
  } = options;
  const parser = {
    index: 0,
    type,
    segmentWildcard: `[^${StringPrototypeReplaceAll(delimiterCodePoint, kEscapeRegexp, '$1')}]+?`,
    tokenList: tokenize(input, 'strict'),
    partList: [],
    pendingFixedValue: '',
    nextNumericName: 0,
  };

  while(parser.index < parser.tokenList.length) {
    const charToken = tryConsumeToken(parser, 'char');
    let nameToken = tryConsumeToken(parser, 'name');
    let regexpOrWildcardToken = tryConsumeRegExpOrWildcardToken(nameToken, parser);
    if (nameToken != null || regexpOrWildcardToken != null) {
      let prefix = '';
      if (charToken != null)
        prefix = charToken.value;
      if (prefix !== '' && prefix !== prefixCodePoint) {
        parser.pendingFixedValue += prefix;
        prefix = '';
      }
      maybeAddPartFromPendingFixedValue(parser);
      const modifierToken = tryConsumeModifierToken(parser);
      addPart(parser, prefix, nameToken, regexpOrWildcardToken, '', modifierToken);
      continue;
    }

    const fixedToken = charToken != null ? charToken : tryConsumeToken(parser, 'escaped-char');

    if (fixedToken != null) {
      parser.pendingFixedValue += fixedToken.value;
      continue;
    }

    const openToken = tryConsumeToken(parser, 'open');
    if (openToken != null) {
      prefix = consumeText(parser);
      nameToken = tryConsumeToken(parser, 'name');
      regexpOrWildcardToken = tryConsumeRegExpOrWildcardToken(nameToken, parser);
      let suffix = consumeText(parser);
      consumeRequiredToken(parser, 'close');
      const modifierToken = tryConsumeModifierToken(parser);
      addPart(parser, prefix, nameToken, regexpOrWildcardToken, suffix, modifierToken);
      continue;
    }

    maybeAddPartFromPendingFixedValue(parser);
    consumeRequiredToken(parser, 'end');
  }

  return parser.partList;
}

function generateRegexAndNameList(partList, options) {
  const {
    delimiterCodePoint,
  } = options;
  let regexString = '^';
  let nameList = [];
  for (let n = 0; n < partList.length; n++) {
    const part = partList[n];
    if (part.type === 'fixed-text') {
      const escapedValue = StringPrototypeReplaceAll(part.value, kEscapeRegexp, '$1');
      regexString += part.modifier === 'none' ?
        escapedValue :
        `(?:${escapedValue})${kModifiers[part.modifier] || ''}`;
      continue;
    }
    // TODO(@jasnell): assert part name is not empty string
    ArrayPrototypePush(nameList, part.name);
    let regexValue = part.value;
    if (part.type === 'segment-wildcard') {
      regexValue = `([^${StringPrototypeReplaceAll(delimiterCodePoint, kEscapeRegexp, '$1')}]+?)`
    } else if (part.type === 'full-wildcard') {
      regexValue = '.*';
    }
    if (part.prefix === '' && part.suffix === '') {
      regexString += part.modifier === 'none' || part.modifier === 'optional' ?
        `(${regexValue})${kModifiers[part.modifier] || ''}` :
        `((?:${regexValue})${kModifiers[part.modifier] || ''})`;
      continue;
    }
    const pfx = StringPrototypeReplaceAll(part.prefix, kEscapeRegexp, '$1');
    const sfx = StringPrototypeReplaceAll(part.suffix, kEscapeRegexp, '$1');
    if (part.modifier === 'none' || part.modifier === 'optional') {
      regexString += `(?:${pfx}(${regexValue})${sfx})${kModifiers[part.modifier] || ''}`;
      continue;
    }
    // TODO(@jasnell): assert part.modifier is 'zero-or-more' or 'one-or-more'
    //                 assert part.prefix and part.suffix are not empty string
    regexString += `(?:${pfx}((?:${regexValue})(?:${sfx}${pfx}(?:{regexValue}))*)${sfx})`;
    if (part.modifier === 'zero-or-more') regexString += '?';
  }
  regexString += '$';
  return { regexString, nameList };
}

function generatePatternString(partList, options) {
  let result = '';
  const {
    delimiterCodePoint,
    prefixCodePoint,
  } = options;

  function escapePatternString(value) {
    // TODO(@jasnell): assert value is an ascii string
    let result = '';
    let index = 0;
    while (index < value.length) {
      const c = value[index++];
      switch (c) {
        case '+':
        case '*':
        case '?':
        case ':':
        case '{':
        case '}':
        case '(':
        case ')':
        case '\\':
          result += '\\';
      }
      result += c;
    }
    return result;
  }

  for (let n = 0; n < partList.length; n++) {
    const part = partList[n];
    if (part.type === 'fixed-text') {
      if (part.modifier === 'none') {
        result += escapePatternString(part.value);
        continue;
      }
      result += `{${escapePatternString(part.value)}}${kModifiers[part.modifier] || ''}`;
      continue;
    }
    const needsGrouping =
      part.suffix !== '' ||
      (part.prefix !== '' && part.prefix !== prefixCodePoint);
    // TODO(@jasnell): assert part.name !== '' and part.name !== null
    const customName = RegExpPrototypeTest(/^[^\d]/, part.name);
    if (needsGrouping) result += '{';
    result += escapePatternString(part.prefix);
    if (customName) result += `:${part.name}`;
    if (part.type === 'regexp')
      result += `(${part.value})`;
    else if (part.type === 'segment-wildcard' && !customName)
      result += `([^${StringPrototypeReplaceAll(delimiterCodePoint, kEscapeRegexp, '$1')}]+?)`
    else if (part.type === 'full-wildcard')
      result += customName ? '(.*)' : '*';
    result += escapePatternString(part.suffix);
    if (needsGrouping) result += '}';
    result += kModifiers[part.modifier] || '';
  }
  return result;
}

function compileAndCanonicalize(component, type, options = {}) {
  if (component == null)
    component = '*';

  const partList = parsePatternString(component, type, options);
  const {
    regexString,
    nameList,
  } = generateRegexAndNameList(partList, options);

  let regex;
  try {
    regex = new RegExp(regexString, 'u');
  } catch (err) {
    throw new ERR_INVALID_ARG_VALUE(`input.${type}`, component);
  }

  const pattern = generatePatternString(partList, options);

  return {
    pattern,
    regex,
    nameList,
  };
}

function protocolMatchesSpecialScheme(protocol) {
  const schemes = ObjectKeys(kSpecialSchemes);
  for (let n = 0; n < schemes.length; n++) {
    const scheme = schemes[n];
    if (RegExpPrototypeExec(protocol.regex, scheme))
      return true;
  }
  return false;
}

function match(urlPattern, input, baseURLString) {
  let protocol = '';
  let username = '';
  let password = '';
  let hostname = '';
  let port = '';
  let pathname = '';
  let search = '';
  let hash = '';
  const inputs = [ input ];

  if (typeof input === 'object' && input != null) {
    if (baseURLString !== undefined) {
      // TODO(@jasnell): Throw a proper error
      throw new TypeError('baseURL should not be provided');
    }
    try {
      const applyResult =
        processURLPatternInit(
          input,
          'url',
          protocol,
          username,
          password,
          hostname,
          port,
          pathname,
          search,
          hash);
      protocol = applyResult.protocol;
      username = applyResult.username;
      password = applyResult.password;
      hostname = applyResult.hostname;
      port = applyResult.port;
      pathname = applyResult.pathname;
      search = applyResult.search;
      hash = applyResult.hash;
    } catch {
      return null;
    }
  } else {
    let baseURL;
    let url;
    try {
      if (baseURLString !== undefined) {
        baseURL = new URL(baseURLString);
        ArrayPrototypePush(inputs, baseURLString);
      }
      url = new URL(input, baseURL);
    } catch {
      return null;
    }
    protocol = url.protocol.slice(0, -1);  // Trim the trailing :
    username = url.username;
    password = url.password;
    hostname = url.host;
    port = url.port;
    pathname = url.pathname;
    search = url.search.slice(1);  // Trim the leading ?
    hash = url.hash.slice(1);  // Trim the leading #
  }

  const protocolExecResult = RegExpPrototypeExec(urlPattern.protocol.regex, protocol);
  const usernameExecResult = RegExpPrototypeExec(urlPattern.username.regex, username);
  const passwordExecResult = RegExpPrototypeExec(urlPattern.password.regex, password);
  const hostnameExecResult = RegExpPrototypeExec(urlPattern.hostname.regex, hostname);
  const portExecResult = RegExpPrototypeExec(urlPattern.port.regex, port);
  const pathnameExecResult = RegExpPrototypeExec(urlPattern.pathname.regex, pathname);
  const searchExecResult = RegExpPrototypeExec(urlPattern.search.regex, search);
  const hashExecResult = RegExpPrototypeExec(urlPattern.hash.regex, hash);
  if (protocolExecResult == null &&
      usernameExecResult == null &&
      passwordExecResult == null &&
      hostnameExecResult == null &&
      portExecResult == null &&
      pathnameExecResult == null &&
      searchExecResult == null &&
      hashExecResult == null) {
    return null;
  }
  return {
    inputs,
    protocol: createResult(urlPattern.protocol, protocol, protocolExecResult),
    username: createResult(urlPattern.username, username, usernameExecResult),
    password: createResult(urlPattern.password, password, passwordExecResult),
    hostname: createResult(urlPattern.hostname, hostname, hostnameExecResult),
    port: createResult(urlPattern.port, port, portExecResult),
    pathname: createResult(urlPattern.pathname, pathname, pathnameExecResult),
    search: createResult(urlPattern.search, search, searchExecResult),
    hash: createResult(urlPattern.hash, hash, hashExecResult),
  };
}

function createResult(component, input, result) {
  const groups = {};
  let index = 1;
  while (index < result.length) {
    const name = component.nameList[index - 1];
    const value = `${result[index++]}`;
    groups[name] = value;
  }
  return {
    input,
    groups,
  }
}

class URLPattern {
  /**
   * @param {URLPatternInput} [input]
   * @param {string} [baseURL]
   */
  constructor(input = {}, baseURL = undefined) {
    if (typeof input === 'string') {
      input = parseConstructorString(toUsvString(input), baseURL);
    } else {
      validateObject(input, 'input');
      if (baseURL !== undefined)
        throw new ERR_INVALID_ARG_VALUE('baseURL', baseURL);
    }
    input = processURLPatternInit(input, 'pattern');

    const defaultPort = kSpecialSchemes[input.protocol];
    if (defaultPort !== undefined && `${input.port}` === `${defaultPort}`)
      input.port = '';

    const protocol = compileAndCanonicalize(input.protocol, 'protocol');

    this[kState] = {
      protocol,
      username: compileAndCanonicalize(input.username, 'username'),
      password: compileAndCanonicalize(input.password, 'password'),
      hostname: isIPv6(input.hostname) ?
        compileAndCanonicalize(input.hostname, 'hostname-ipv6') :
        compileAndCanonicalize(input.hostname, 'hostname'),
      port: compileAndCanonicalize(input.port, 'port'),
      pathname: protocolMatchesSpecialScheme(protocol) ?
        compileAndCanonicalize(input.pathname, 'pathname', {
          delimiterCodePoint: '/',
          prefixCodePoint: '/',
        }) :
        compileAndCanonicalize(input.pathname, 'pathname'),
      search: compileAndCanonicalize(input.search, 'search'),
      hash: compileAndCanonicalize(input.hash, 'hash'),
    };
  }

  /**
   * @param {URLPatternInput} [input]
   * @param {string} [baseURL]
   * @returns {boolean}
   */
  test(input, baseURL) {
    if (this[kState] === undefined)
      throw new ERR_INVALID_THIS('URLPattern');
    return match(this[kState], input, baseURL) != null;
  }

  /**
   * @param {URLPatternInput} [input]
   * @param {string} [baseURL]
   * @param {URLPatternResult}
   */
  exec(input, baseURL) {
    if (this[kState] === undefined)
      throw new ERR_INVALID_THIS('URLPattern');
    return match(this[kState], input, baseURL);
  }

  /** @readonly @type {string} */
  get protocol() {
    if (this[kState] === undefined)
      throw new ERR_INVALID_THIS('URLPattern');
    return this[kState].protocol.pattern;
  }

  /** @readonly @type {string} */
  get username() {
    if (this[kState] === undefined)
      throw new ERR_INVALID_THIS('URLPattern');
    return this[kState].username.pattern;
  }

  /** @readonly @type {string} */
  get password() {
    if (this[kState] === undefined)
      throw new ERR_INVALID_THIS('URLPattern');
    return this[kState].password.pattern;
  }

  /** @readonly @type {string} */
  get hostname() {
    if (this[kState] === undefined)
      throw new ERR_INVALID_THIS('URLPattern');
    return this[kState].hostname.pattern;
  }

  /** @readonly @type {string} */
  get port() {
    if (this[kState] === undefined)
      throw new ERR_INVALID_THIS('URLPattern');
    return this[kState].port.pattern;
  }

  /** @readonly @type {string} */
  get pathname() {
    if (this[kState] === undefined)
      throw new ERR_INVALID_THIS('URLPattern');
    return this[kState].pathname.pattern;
  }

  /** @readonly @type {string} */
  get search() {
    if (this[kState] === undefined)
      throw new ERR_INVALID_THIS('URLPattern');
    return this[kState].search.pattern;
  }

  /** @readonly @type {string} */
  get hash() {
    if (this[kState] === undefined)
      throw new ERR_INVALID_THIS('URLPattern');
    return this[kState].hash.pattern;
  }

  [kInspect](depth, options) {
    if (this[kState] === undefined)
      throw new ERR_INVALID_THIS('URLPattern');

    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1
    };

    return `URLPattern ${inspect({
      protocol: this.protocol,
      username: this.username,
      password: this.password,
      hostname: this.hostname,
      port: this.port,
      pathname: this.pathname,
      search: this.search,
      hash: this.hash,
    }, opts)}`;
  }
}

ObjectDefineProperties(URLPattern.prototype, {
  prototcol: { enumerable: true, },
  username: { enumerable: true, },
  password: { enumerable: true, },
  hostname: { enumerable: true, },
  port: { enumerable: true, },
  pathname: { enumerable: true, },
  search: { enumerable: true, },
  hash: { enumerable: true, },
});

module.exports = URLPattern;
