// RegExp patterns inspired by https://github.com/speed-highlight/core/blob/249ad8fe9bf3eb951eb263b5b3c35887bbbbb2eb/src/languages/js.js

'use strict';
const {
  RegExpPrototypeExec,
  StringPrototypeSlice,
} = primordials;

const { inspect } = require('internal/util/inspect');

const matchers = [
  {
    // Single line comment or multi line comment
    color: inspect.colors.grey,
    patterns: [
      /\/\/.*\n?|\/\*((?!\*\/)[^])*(\*\/)?/g,
    ],
  },
  {
    // String
    color: inspect.colors[inspect.styles.string],
    patterns: [
      // 'string' or "string"
      /(["'])(\\[^]|(?!\1)[^\r\n\\])*\1?/g,
      // `string`
      /`((?!`)[^]|\\[^])*`?/g,
    ],
  },
  {
    // Keywords
    color: inspect.colors[inspect.styles.special],
    patterns: [
      /=>|\b(as|async|await|break|case|catch|class|const|continue|debugger|default|delete|do|else|export|extends|finally|for|from|function|if|import|in|instanceof|let|new|of|return|static|switch|throw|try|typeof|var|void|while|with|yield|this|super)\b/g,
    ],
  },
  {
    // RegExp
    color: inspect.colors[inspect.styles.regexp],
    patterns: [
      // eslint-disable-next-line node-core/no-unescaped-regexp-dot
      /\/((?!\/)[^\r\n\\]|\\.)+\/[dgimsuy]*/g,
    ],
  },
  {
    // Number
    color: inspect.colors[inspect.styles.number],
    patterns: [
      /(\.e?|\b)\d(e-|[\d.oxa-fA-F_])*(\.|\b)/g,
      /\bNaN\b/g,
    ],
  },
  {
    // Undefined
    color: inspect.colors[inspect.styles.undefined],
    patterns: [
      /\bundefined\b/g,
    ],
  },
  {
    // Null
    color: inspect.colors[inspect.styles.null],
    patterns: [
      /\bnull\b/g,
    ],
  },
  {
    // Boolean
    color: inspect.colors[inspect.styles.boolean],
    patterns: [
      /\b(true|false)\b/g,
    ],
  },
  {
    // Operator
    color: inspect.colors[inspect.styles.special],
    patterns: [
      /[/*+:?&|%^~=!,<>.^-]+/g,
    ],
  },
  {
    // Identifier
    color: inspect.colors.white,
    patterns: [
      /\b[A-Z][\w_]*\b/g,
    ],
  },
  {
    // Function Declaration
    color: inspect.colors[inspect.styles.special],
    patterns: [
      // eslint-disable-next-line no-useless-escape
      /[a-zA-Z$_][\w$_]*(?=\s*((\?\.)?\s*\(|=\s*(\(?[\w,{}\[\])]+\)? =>|function\b)))/g,
    ],
  },
];

function highlight(code) {
  let tokens = '';

  for (let index = 0; index < code.length;) {
    let match = null;
    let matchIndex = code.length;
    let matchType = null;

    for (const { color, patterns } of matchers) {
      for (const pattern of patterns) {
        pattern.lastIndex = index;
        const result = RegExpPrototypeExec(pattern, code);

        if (result && result.index < matchIndex) {
          match = result[0];
          matchIndex = result.index;
          matchType = color;
        }
      }
    }

    if (matchIndex > index) {
      tokens += StringPrototypeSlice(code, index, matchIndex);
    }

    if (match) {
      tokens += colorize(match, matchType);
      index = matchIndex + match.length;
    } else {
      break;
    }
  }

  return tokens;
}


function colorize(text, color) {
  if (color === inspect.colors.white) return text;
  return `\u001b[${color[0]}m${text}\u001b[${color[1]}m`;
}

module.exports = highlight;
