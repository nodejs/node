"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.Identifier = Identifier;
exports.ArgumentPlaceholder = ArgumentPlaceholder;
exports.SpreadElement = exports.RestElement = RestElement;
exports.ObjectPattern = exports.ObjectExpression = ObjectExpression;
exports.ObjectMethod = ObjectMethod;
exports.ObjectProperty = ObjectProperty;
exports.ArrayPattern = exports.ArrayExpression = ArrayExpression;
exports.RecordExpression = RecordExpression;
exports.TupleExpression = TupleExpression;
exports.RegExpLiteral = RegExpLiteral;
exports.BooleanLiteral = BooleanLiteral;
exports.NullLiteral = NullLiteral;
exports.NumericLiteral = NumericLiteral;
exports.StringLiteral = StringLiteral;
exports.BigIntLiteral = BigIntLiteral;
exports.DecimalLiteral = DecimalLiteral;
exports.PipelineTopicExpression = PipelineTopicExpression;
exports.PipelineBareFunction = PipelineBareFunction;
exports.PipelinePrimaryTopicReference = PipelinePrimaryTopicReference;

var t = _interopRequireWildcard(require("@babel/types"));

var _jsesc = _interopRequireDefault(require("jsesc"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _getRequireWildcardCache() { if (typeof WeakMap !== "function") return null; var cache = new WeakMap(); _getRequireWildcardCache = function () { return cache; }; return cache; }

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } if (obj === null || typeof obj !== "object" && typeof obj !== "function") { return { default: obj }; } var cache = _getRequireWildcardCache(); if (cache && cache.has(obj)) { return cache.get(obj); } var newObj = {}; var hasPropertyDescriptor = Object.defineProperty && Object.getOwnPropertyDescriptor; for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = hasPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : null; if (desc && (desc.get || desc.set)) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } newObj.default = obj; if (cache) { cache.set(obj, newObj); } return newObj; }

function Identifier(node) {
  this.exactSource(node.loc, () => {
    this.word(node.name);
  });
}

function ArgumentPlaceholder() {
  this.token("?");
}

function RestElement(node) {
  this.token("...");
  this.print(node.argument, node);
}

function ObjectExpression(node) {
  const props = node.properties;
  this.token("{");
  this.printInnerComments(node);

  if (props.length) {
    this.space();
    this.printList(props, node, {
      indent: true,
      statement: true
    });
    this.space();
  }

  this.token("}");
}

function ObjectMethod(node) {
  this.printJoin(node.decorators, node);

  this._methodHead(node);

  this.space();
  this.print(node.body, node);
}

function ObjectProperty(node) {
  this.printJoin(node.decorators, node);

  if (node.computed) {
    this.token("[");
    this.print(node.key, node);
    this.token("]");
  } else {
    if (t.isAssignmentPattern(node.value) && t.isIdentifier(node.key) && node.key.name === node.value.left.name) {
      this.print(node.value, node);
      return;
    }

    this.print(node.key, node);

    if (node.shorthand && t.isIdentifier(node.key) && t.isIdentifier(node.value) && node.key.name === node.value.name) {
      return;
    }
  }

  this.token(":");
  this.space();
  this.print(node.value, node);
}

function ArrayExpression(node) {
  const elems = node.elements;
  const len = elems.length;
  this.token("[");
  this.printInnerComments(node);

  for (let i = 0; i < elems.length; i++) {
    const elem = elems[i];

    if (elem) {
      if (i > 0) this.space();
      this.print(elem, node);
      if (i < len - 1) this.token(",");
    } else {
      this.token(",");
    }
  }

  this.token("]");
}

function RecordExpression(node) {
  const props = node.properties;
  let startToken;
  let endToken;

  if (this.format.recordAndTupleSyntaxType === "bar") {
    startToken = "{|";
    endToken = "|}";
  } else if (this.format.recordAndTupleSyntaxType === "hash") {
    startToken = "#{";
    endToken = "}";
  } else {
    throw new Error(`The "recordAndTupleSyntaxType" generator option must be "bar" or "hash" (${JSON.stringify(this.format.recordAndTupleSyntaxType)} received).`);
  }

  this.token(startToken);
  this.printInnerComments(node);

  if (props.length) {
    this.space();
    this.printList(props, node, {
      indent: true,
      statement: true
    });
    this.space();
  }

  this.token(endToken);
}

function TupleExpression(node) {
  const elems = node.elements;
  const len = elems.length;
  let startToken;
  let endToken;

  if (this.format.recordAndTupleSyntaxType === "bar") {
    startToken = "[|";
    endToken = "|]";
  } else if (this.format.recordAndTupleSyntaxType === "hash") {
    startToken = "#[";
    endToken = "]";
  } else {
    throw new Error(`${this.format.recordAndTupleSyntaxType} is not a valid recordAndTuple syntax type`);
  }

  this.token(startToken);
  this.printInnerComments(node);

  for (let i = 0; i < elems.length; i++) {
    const elem = elems[i];

    if (elem) {
      if (i > 0) this.space();
      this.print(elem, node);
      if (i < len - 1) this.token(",");
    }
  }

  this.token(endToken);
}

function RegExpLiteral(node) {
  this.word(`/${node.pattern}/${node.flags}`);
}

function BooleanLiteral(node) {
  this.word(node.value ? "true" : "false");
}

function NullLiteral() {
  this.word("null");
}

function NumericLiteral(node) {
  const raw = this.getPossibleRaw(node);
  const opts = this.format.jsescOption;
  const value = node.value + "";

  if (opts.numbers) {
    this.number((0, _jsesc.default)(node.value, opts));
  } else if (raw == null) {
    this.number(value);
  } else if (this.format.minified) {
    this.number(raw.length < value.length ? raw : value);
  } else {
    this.number(raw);
  }
}

function StringLiteral(node) {
  const raw = this.getPossibleRaw(node);

  if (!this.format.minified && raw != null) {
    this.token(raw);
    return;
  }

  const val = (0, _jsesc.default)(node.value, Object.assign(this.format.jsescOption, this.format.jsonCompatibleStrings && {
    json: true
  }));
  return this.token(val);
}

function BigIntLiteral(node) {
  const raw = this.getPossibleRaw(node);

  if (!this.format.minified && raw != null) {
    this.word(raw);
    return;
  }

  this.word(node.value + "n");
}

function DecimalLiteral(node) {
  const raw = this.getPossibleRaw(node);

  if (!this.format.minified && raw != null) {
    this.word(raw);
    return;
  }

  this.word(node.value + "m");
}

function PipelineTopicExpression(node) {
  this.print(node.expression, node);
}

function PipelineBareFunction(node) {
  this.print(node.callee, node);
}

function PipelinePrimaryTopicReference() {
  this.token("#");
}