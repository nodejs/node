"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.types = exports.TokContext = undefined;

var _classCallCheck2 = require("babel-runtime/helpers/classCallCheck");

var _classCallCheck3 = _interopRequireDefault(_classCallCheck2);

var _types = require("./types");

var _whitespace = require("../util/whitespace");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

// The algorithm used to determine whether a regexp can appear at a
// given point in the program is loosely based on sweet.js' approach.
// See https://github.com/mozilla/sweet.js/wiki/design

var TokContext = exports.TokContext = function TokContext(token, isExpr, preserveSpace, override) {
  (0, _classCallCheck3.default)(this, TokContext);

  this.token = token;
  this.isExpr = !!isExpr;
  this.preserveSpace = !!preserveSpace;
  this.override = override;
};

var types = exports.types = {
  braceStatement: new TokContext("{", false),
  braceExpression: new TokContext("{", true),
  templateQuasi: new TokContext("${", true),
  parenStatement: new TokContext("(", false),
  parenExpression: new TokContext("(", true),
  template: new TokContext("`", true, true, function (p) {
    return p.readTmplToken();
  }),
  functionExpression: new TokContext("function", true)
};

// Token-specific context update code

_types.types.parenR.updateContext = _types.types.braceR.updateContext = function () {
  if (this.state.context.length === 1) {
    this.state.exprAllowed = true;
    return;
  }

  var out = this.state.context.pop();
  if (out === types.braceStatement && this.curContext() === types.functionExpression) {
    this.state.context.pop();
    this.state.exprAllowed = false;
  } else if (out === types.templateQuasi) {
    this.state.exprAllowed = true;
  } else {
    this.state.exprAllowed = !out.isExpr;
  }
};

_types.types.name.updateContext = function (prevType) {
  this.state.exprAllowed = false;

  if (prevType === _types.types._let || prevType === _types.types._const || prevType === _types.types._var) {
    if (_whitespace.lineBreak.test(this.input.slice(this.state.end))) {
      this.state.exprAllowed = true;
    }
  }
};

_types.types.braceL.updateContext = function (prevType) {
  this.state.context.push(this.braceIsBlock(prevType) ? types.braceStatement : types.braceExpression);
  this.state.exprAllowed = true;
};

_types.types.dollarBraceL.updateContext = function () {
  this.state.context.push(types.templateQuasi);
  this.state.exprAllowed = true;
};

_types.types.parenL.updateContext = function (prevType) {
  var statementParens = prevType === _types.types._if || prevType === _types.types._for || prevType === _types.types._with || prevType === _types.types._while;
  this.state.context.push(statementParens ? types.parenStatement : types.parenExpression);
  this.state.exprAllowed = true;
};

_types.types.incDec.updateContext = function () {
  // tokExprAllowed stays unchanged
};

_types.types._function.updateContext = function () {
  if (this.curContext() !== types.braceStatement) {
    this.state.context.push(types.functionExpression);
  }

  this.state.exprAllowed = false;
};

_types.types.backQuote.updateContext = function () {
  if (this.curContext() === types.template) {
    this.state.context.pop();
  } else {
    this.state.context.push(types.template);
  }
  this.state.exprAllowed = false;
};