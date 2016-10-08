"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.tokTypes = undefined;
exports.parse = parse;

var _parser = require("./parser");

var _parser2 = _interopRequireDefault(_parser);

require("./parser/util");

require("./parser/statement");

require("./parser/lval");

require("./parser/expression");

require("./parser/node");

require("./parser/location");

require("./parser/comments");

var _types = require("./tokenizer/types");

require("./tokenizer");

require("./tokenizer/context");

var _flow = require("./plugins/flow");

var _flow2 = _interopRequireDefault(_flow);

var _jsx = require("./plugins/jsx");

var _jsx2 = _interopRequireDefault(_jsx);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

_parser.plugins.flow = _flow2.default;
_parser.plugins.jsx = _jsx2.default;

function parse(input, options) {
  return new _parser2.default(options, input).parse();
}

exports.tokTypes = _types.types;