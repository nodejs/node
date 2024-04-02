"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.statements = exports.statement = exports.smart = exports.program = exports.expression = exports.default = void 0;
var formatters = require("./formatters.js");
var _builder = require("./builder.js");
const smart = (0, _builder.default)(formatters.smart);
exports.smart = smart;
const statement = (0, _builder.default)(formatters.statement);
exports.statement = statement;
const statements = (0, _builder.default)(formatters.statements);
exports.statements = statements;
const expression = (0, _builder.default)(formatters.expression);
exports.expression = expression;
const program = (0, _builder.default)(formatters.program);
exports.program = program;
var _default = Object.assign(smart.bind(undefined), {
  smart,
  statement,
  statements,
  expression,
  program,
  ast: smart.ast
});
exports.default = _default;

//# sourceMappingURL=index.js.map
