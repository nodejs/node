'use strict';

const { inspect } = require('internal/util/inspect');

const matchers = [
  {
    // Single line comment or multi line comment
    color: inspect.colors[inspect.styles.special],
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
    // Keyword
    color: inspect.colors[inspect.styles.special],
    patterns: [
      /=>|\b(this|set|get|as|async|await|break|case|catch|class|const|constructor|continue|debugger|default|delete|do|else|enum|export|extends|finally|for|from|function|if|implements|import|in|instanceof|interface|let|var|of|new|package|private|protected|public|return|static|super|switch|throw|throws|try|typeof|void|while|with|yield)\b/g,
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
      /\b(NaN|null|undefined|[A-Z][A-Z_]*)\b/g,
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
    color: inspect.colors[inspect.styles.special],
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
  const tokens = [];

  for (let index = 0; index < code.length;) {
    let match = null;
    let matchIndex = code.length;
    let matchType = null;

    for (const { color, patterns } of matchers) {
      for (const pattern of patterns) {
        pattern.lastIndex = index;
        const result = pattern.exec(code);

        if (result && result.index < matchIndex) {
          match = result[0];
          matchIndex = result.index;
          matchType = color;
        }
      }
    }

    if (matchIndex > index) {
      tokens.push(code.slice(index, matchIndex));
    }

    if (match) {
      tokens.push(colorize(match, matchType));
      index = matchIndex + match.length;
    } else {
      break;
    }
  }

  return tokens.join('');
}


function colorize(text, color) {
  return `\u001b[${color[0]}m${text}\u001b[${color[1]}m`;
}

module.exports = highlight;
