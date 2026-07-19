'use strict';

const {
  FunctionPrototypeBind,
  StringPrototypeSlice,
} = primordials;

const { stylizeWithColor } = require('internal/util/inspect');
const { Parser } = require('internal/deps/acorn/acorn/dist/acorn');

const tokenizer = FunctionPrototypeBind(Parser.tokenizer, Parser);

function tokenStyle(token) {
  const { label, keyword } = token.type;

  if (keyword !== undefined) {
    if (label === 'true' || label === 'false') return 'boolean';
    if (label === 'null') return 'null';
    if (label === 'import' || label === 'export') return 'module';
    return 'special';
  }

  switch (label) {
    case 'name':
      switch (token.value) {
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
    case 'num':
      return 'number';
    case 'bigint':
      return 'bigint';
    case 'string':
    case 'template':
    case '`':
      return 'string';
    case 'regexp':
      return 'regexp';
    default:
      return undefined;
  }
}

function highlight(code) {
  if (code.length === 0) return code;

  let result = '';
  let offset = 0;
  function write(start, end, style) {
    result += 
      StringPrototypeSlice(code, offset, start) +
      stylizeWithColor(StringPrototypeSlice(code, start, end), style);
    offset = end;
  }

  try {
    const iterator = tokenizer(code, {
      __proto__: null,
      allowHashBang: true,
      ecmaVersion: 'latest',
      onComment(_block, _text, start, end) {
        write(start, end, 'undefined');
      },
    });

    for (const token of iterator) {
      const style = tokenStyle(token);
      if (style !== undefined) {
        write(token.start, token.end, style);
      }
    }
  } catch {
    // Acorn throws for unfinished strings, templates, and comments. Tokens and
    // comments reported before that point are still useful while typing.
  }

  return result + StringPrototypeSlice(code, offset);
}

module.exports = { highlight };
