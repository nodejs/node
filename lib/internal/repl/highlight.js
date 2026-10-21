'use strict';

const {
  StringPrototypeSlice,
} = primordials;

const { stylizeWithColor } = require('internal/util/inspect');

let tokenize;

function tokenStyle(kind, text) {
  switch (kind) {
    case 'keyword':
      if (text === 'true' || text === 'false') return 'boolean';
      if (text === 'null') return 'null';
      if (text === 'import' || text === 'export') return 'module';
      return 'special';
    case 'identifier':
      switch (text) {
        case 'undefined':
          return 'undefined';
        case 'NaN':
        case 'Infinity':
          return 'number';
        case 'Symbol':
          return 'symbol';
        case 'Date':
          return 'date';
        default:
          return undefined;
      }
    case 'number':
      return 'number';
    case 'bigint':
      return 'bigint';
    case 'string':
    case 'template':
      return 'string';
    case 'regexp':
      return 'regexp';
    case 'comment':
      return 'undefined';
    default:
      return undefined;
  }
}

function highlight(code) {
  if (code.length === 0) return code;

  // Lazy load Amaro, it is only needed once the user starts typing.
  tokenize ??= require('internal/deps/amaro/dist/internal').tokenize;

  let tokens;
  try {
    tokens = tokenize(code);
  } catch {
    return code;
  }

  let result = '';
  let offset = 0;
  // Amaro stops tokenizing at the first lexical or syntax error, for example
  // an unfinished string, template, or comment. The tokens reported before
  // that point are still useful while typing; the rest of the input is left
  // untouched.
  for (let i = 0; i < tokens.length; i++) {
    const { kind, start, end } = tokens[i];
    if (start < offset) continue;
    const text = StringPrototypeSlice(code, start, end);
    const style = tokenStyle(kind, text);
    if (style !== undefined) {
      result += StringPrototypeSlice(code, offset, start) +
                stylizeWithColor(text, style);
      offset = end;
    }
  }

  return result + StringPrototypeSlice(code, offset);
}

module.exports = { highlight };
