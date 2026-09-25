"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.combinator = exports.word = exports.comment = exports.str = exports.tab = exports.newline = exports.feed = exports.cr = exports.backslash = exports.bang = exports.slash = exports.doubleQuote = exports.singleQuote = exports.space = exports.greaterThan = exports.pipe = exports.equals = exports.plus = exports.caret = exports.tilde = exports.dollar = exports.closeSquare = exports.openSquare = exports.closeParenthesis = exports.openParenthesis = exports.semicolon = exports.colon = exports.comma = exports.at = exports.asterisk = exports.ampersand = void 0;
exports.ampersand = 38; // `&`.charCodeAt(0);
exports.asterisk = 42; // `*`.charCodeAt(0);
exports.at = 64; // `@`.charCodeAt(0);
exports.comma = 44; // `,`.charCodeAt(0);
exports.colon = 58; // `:`.charCodeAt(0);
exports.semicolon = 59; // `;`.charCodeAt(0);
exports.openParenthesis = 40; // `(`.charCodeAt(0);
exports.closeParenthesis = 41; // `)`.charCodeAt(0);
exports.openSquare = 91; // `[`.charCodeAt(0);
exports.closeSquare = 93; // `]`.charCodeAt(0);
exports.dollar = 36; // `$`.charCodeAt(0);
exports.tilde = 126; // `~`.charCodeAt(0);
exports.caret = 94; // `^`.charCodeAt(0);
exports.plus = 43; // `+`.charCodeAt(0);
exports.equals = 61; // `=`.charCodeAt(0);
exports.pipe = 124; // `|`.charCodeAt(0);
exports.greaterThan = 62; // `>`.charCodeAt(0);
exports.space = 32; // ` `.charCodeAt(0);
exports.singleQuote = 39; // `'`.charCodeAt(0);
exports.doubleQuote = 34; // `"`.charCodeAt(0);
exports.slash = 47; // `/`.charCodeAt(0);
exports.bang = 33; // `!`.charCodeAt(0);
exports.backslash = 92; // '\\'.charCodeAt(0);
exports.cr = 13; // '\r'.charCodeAt(0);
exports.feed = 12; // '\f'.charCodeAt(0);
exports.newline = 10; // '\n'.charCodeAt(0);
exports.tab = 9; // '\t'.charCodeAt(0);
// Expose aliases primarily for readability.
exports.str = exports.singleQuote;
// No good single character representation!
exports.comment = -1;
exports.word = -2;
exports.combinator = -3;
//# sourceMappingURL=tokenTypes.js.map