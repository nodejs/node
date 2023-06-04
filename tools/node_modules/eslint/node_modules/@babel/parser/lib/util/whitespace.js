"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.isNewLine = isNewLine;
exports.isWhitespace = isWhitespace;
exports.skipWhiteSpaceToLineBreak = exports.skipWhiteSpaceInLine = exports.skipWhiteSpace = exports.lineBreakG = exports.lineBreak = void 0;
const lineBreak = /\r\n?|[\n\u2028\u2029]/;
exports.lineBreak = lineBreak;
const lineBreakG = new RegExp(lineBreak.source, "g");
exports.lineBreakG = lineBreakG;
function isNewLine(code) {
  switch (code) {
    case 10:
    case 13:
    case 8232:
    case 8233:
      return true;
    default:
      return false;
  }
}
const skipWhiteSpace = /(?:\s|\/\/.*|\/\*[^]*?\*\/)*/g;
exports.skipWhiteSpace = skipWhiteSpace;
const skipWhiteSpaceInLine = /(?:[^\S\n\r\u2028\u2029]|\/\/.*|\/\*.*?\*\/)*/g;
exports.skipWhiteSpaceInLine = skipWhiteSpaceInLine;
const skipWhiteSpaceToLineBreak = new RegExp("(?=(" + skipWhiteSpaceInLine.source + "))\\1" + /(?=[\n\r\u2028\u2029]|\/\*(?!.*?\*\/)|$)/.source, "y");
exports.skipWhiteSpaceToLineBreak = skipWhiteSpaceToLineBreak;
function isWhitespace(code) {
  switch (code) {
    case 0x0009:
    case 0x000b:
    case 0x000c:
    case 32:
    case 160:
    case 5760:
    case 0x2000:
    case 0x2001:
    case 0x2002:
    case 0x2003:
    case 0x2004:
    case 0x2005:
    case 0x2006:
    case 0x2007:
    case 0x2008:
    case 0x2009:
    case 0x200a:
    case 0x202f:
    case 0x205f:
    case 0x3000:
    case 0xfeff:
      return true;
    default:
      return false;
  }
}

//# sourceMappingURL=whitespace.js.map
