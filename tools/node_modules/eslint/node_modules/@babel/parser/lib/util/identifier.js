"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.canBeReservedWord = canBeReservedWord;
Object.defineProperty(exports, "isIdentifierChar", {
  enumerable: true,
  get: function () {
    return _helperValidatorIdentifier.isIdentifierChar;
  }
});
Object.defineProperty(exports, "isIdentifierStart", {
  enumerable: true,
  get: function () {
    return _helperValidatorIdentifier.isIdentifierStart;
  }
});
exports.isIteratorStart = isIteratorStart;
Object.defineProperty(exports, "isKeyword", {
  enumerable: true,
  get: function () {
    return _helperValidatorIdentifier.isKeyword;
  }
});
Object.defineProperty(exports, "isReservedWord", {
  enumerable: true,
  get: function () {
    return _helperValidatorIdentifier.isReservedWord;
  }
});
Object.defineProperty(exports, "isStrictBindOnlyReservedWord", {
  enumerable: true,
  get: function () {
    return _helperValidatorIdentifier.isStrictBindOnlyReservedWord;
  }
});
Object.defineProperty(exports, "isStrictBindReservedWord", {
  enumerable: true,
  get: function () {
    return _helperValidatorIdentifier.isStrictBindReservedWord;
  }
});
Object.defineProperty(exports, "isStrictReservedWord", {
  enumerable: true,
  get: function () {
    return _helperValidatorIdentifier.isStrictReservedWord;
  }
});
exports.keywordRelationalOperator = void 0;
var _helperValidatorIdentifier = require("@babel/helper-validator-identifier");
const keywordRelationalOperator = /^in(stanceof)?$/;
exports.keywordRelationalOperator = keywordRelationalOperator;
function isIteratorStart(current, next, next2) {
  return current === 64 && next === 64 && (0, _helperValidatorIdentifier.isIdentifierStart)(next2);
}
const reservedWordLikeSet = new Set(["break", "case", "catch", "continue", "debugger", "default", "do", "else", "finally", "for", "function", "if", "return", "switch", "throw", "try", "var", "const", "while", "with", "new", "this", "super", "class", "extends", "export", "import", "null", "true", "false", "in", "instanceof", "typeof", "void", "delete", "implements", "interface", "let", "package", "private", "protected", "public", "static", "yield", "eval", "arguments", "enum", "await"]);
function canBeReservedWord(word) {
  return reservedWordLikeSet.has(word);
}

//# sourceMappingURL=identifier.js.map
