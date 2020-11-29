"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = _default;

var _convertTokens = _interopRequireDefault(require("./convertTokens"));

var _convertComments = _interopRequireDefault(require("./convertComments"));

var _convertAST = _interopRequireDefault(require("./convertAST"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _default(ast, code) {
  ast.tokens = (0, _convertTokens.default)(ast.tokens, code);
  (0, _convertComments.default)(ast.comments);
  (0, _convertAST.default)(ast, code);
}