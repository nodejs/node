(function(f){if(typeof exports==="object"&&typeof module!=="undefined"){module.exports=f()}else if(typeof define==="function"&&define.amd){define([],f)}else{var g;if(typeof window!=="undefined"){g=window}else if(typeof global!=="undefined"){g=global}else if(typeof self!=="undefined"){g=self}else{g=this}(g.acorn || (g.acorn = {})).loose = f()}})(function(){var define,module,exports;return (function e(t,n,r){function s(o,u){if(!n[o]){if(!t[o]){var a=typeof require=="function"&&require;if(!u&&a)return a(o,!0);if(i)return i(o,!0);var f=new Error("Cannot find module '"+o+"'");throw f.code="MODULE_NOT_FOUND",f}var l=n[o]={exports:{}};t[o][0].call(l.exports,function(e){var n=t[o][1][e];return s(n?n:e)},l,l.exports,e,t,n,r)}return n[o].exports}var i=typeof require=="function"&&require;for(var o=0;o<r.length;o++)s(r[o]);return s})({1:[function(_dereq_,module,exports){
"use strict";

var _interopRequireWildcard = function (obj) { return obj && obj.__esModule ? obj : { "default": obj }; };

exports.parse_dammit = parse_dammit;
exports.__esModule = true;
// Acorn: Loose parser
//
// This module provides an alternative parser (`parse_dammit`) that
// exposes that same interface as `parse`, but will try to parse
// anything as JavaScript, repairing syntax error the best it can.
// There are circumstances in which it will raise an error and give
// up, but they are very rare. The resulting AST will be a mostly
// valid JavaScript AST (as per the [Mozilla parser API][api], except
// that:
//
// - Return outside functions is allowed
//
// - Label consistency (no conflicts, break only to existing labels)
//   is not enforced.
//
// - Bogus Identifier nodes with a name of `"✖"` are inserted whenever
//   the parser got too confused to return anything meaningful.
//
// [api]: https://developer.mozilla.org/en-US/docs/SpiderMonkey/Parser_API
//
// The expected use for this is to *first* try `acorn.parse`, and only
// if that fails switch to `parse_dammit`. The loose parser might
// parse badly indented code incorrectly, so **don't** use it as
// your default parser.
//
// Quite a lot of acorn.js is duplicated here. The alternative was to
// add a *lot* of extra cruft to that file, making it less readable
// and slower. Copying and editing the code allowed me to make
// invasive changes and simplifications without creating a complicated
// tangle.

var acorn = _interopRequireWildcard(_dereq_(".."));

var _state = _dereq_("./state");

var LooseParser = _state.LooseParser;

_dereq_("./tokenize");

_dereq_("./parseutil");

_dereq_("./statement");

_dereq_("./expression");

exports.LooseParser = _state.LooseParser;

acorn.defaultOptions.tabSize = 4;

function parse_dammit(input, options) {
  var p = new LooseParser(input, options);
  p.next();
  return p.parseTopLevel();
}

acorn.parse_dammit = parse_dammit;
acorn.LooseParser = LooseParser;

},{"..":2,"./expression":3,"./parseutil":4,"./state":5,"./statement":6,"./tokenize":7}],2:[function(_dereq_,module,exports){
"use strict";

module.exports = typeof acorn != "undefined" ? acorn : _dereq_("./acorn");

},{}],3:[function(_dereq_,module,exports){
"use strict";

var LooseParser = _dereq_("./state").LooseParser;

var isDummy = _dereq_("./parseutil").isDummy;

var tt = _dereq_("..").tokTypes;

var lp = LooseParser.prototype;

lp.checkLVal = function (expr, binding) {
  if (!expr) return expr;
  switch (expr.type) {
    case "Identifier":
      return expr;

    case "MemberExpression":
      return binding ? this.dummyIdent() : expr;

    case "ParenthesizedExpression":
      expr.expression = this.checkLVal(expr.expression, binding);
      return expr;

    // FIXME recursively check contents
    case "ObjectPattern":
    case "ArrayPattern":
    case "RestElement":
    case "AssignmentPattern":
      if (this.options.ecmaVersion >= 6) return expr;

    default:
      return this.dummyIdent();
  }
};

lp.parseExpression = function (noIn) {
  var start = this.storeCurrentPos();
  var expr = this.parseMaybeAssign(noIn);
  if (this.tok.type === tt.comma) {
    var node = this.startNodeAt(start);
    node.expressions = [expr];
    while (this.eat(tt.comma)) node.expressions.push(this.parseMaybeAssign(noIn));
    return this.finishNode(node, "SequenceExpression");
  }
  return expr;
};

lp.parseParenExpression = function () {
  this.pushCx();
  this.expect(tt.parenL);
  var val = this.parseExpression();
  this.popCx();
  this.expect(tt.parenR);
  return val;
};

lp.parseMaybeAssign = function (noIn) {
  var start = this.storeCurrentPos();
  var left = this.parseMaybeConditional(noIn);
  if (this.tok.type.isAssign) {
    var node = this.startNodeAt(start);
    node.operator = this.tok.value;
    node.left = this.tok.type === tt.eq ? this.toAssignable(left) : this.checkLVal(left);
    this.next();
    node.right = this.parseMaybeAssign(noIn);
    return this.finishNode(node, "AssignmentExpression");
  }
  return left;
};

lp.parseMaybeConditional = function (noIn) {
  var start = this.storeCurrentPos();
  var expr = this.parseExprOps(noIn);
  if (this.eat(tt.question)) {
    var node = this.startNodeAt(start);
    node.test = expr;
    node.consequent = this.parseMaybeAssign();
    node.alternate = this.expect(tt.colon) ? this.parseMaybeAssign(noIn) : this.dummyIdent();
    return this.finishNode(node, "ConditionalExpression");
  }
  return expr;
};

lp.parseExprOps = function (noIn) {
  var start = this.storeCurrentPos();
  var indent = this.curIndent,
      line = this.curLineStart;
  return this.parseExprOp(this.parseMaybeUnary(noIn), start, -1, noIn, indent, line);
};

lp.parseExprOp = function (left, start, minPrec, noIn, indent, line) {
  if (this.curLineStart != line && this.curIndent < indent && this.tokenStartsLine()) return left;
  var prec = this.tok.type.binop;
  if (prec != null && (!noIn || this.tok.type !== tt._in)) {
    if (prec > minPrec) {
      var node = this.startNodeAt(start);
      node.left = left;
      node.operator = this.tok.value;
      this.next();
      if (this.curLineStart != line && this.curIndent < indent && this.tokenStartsLine()) {
        node.right = this.dummyIdent();
      } else {
        var rightStart = this.storeCurrentPos();
        node.right = this.parseExprOp(this.parseMaybeUnary(noIn), rightStart, prec, noIn, indent, line);
      }
      this.finishNode(node, /&&|\|\|/.test(node.operator) ? "LogicalExpression" : "BinaryExpression");
      return this.parseExprOp(node, start, minPrec, noIn, indent, line);
    }
  }
  return left;
};

lp.parseMaybeUnary = function (noIn) {
  if (this.tok.type.prefix) {
    var node = this.startNode(),
        update = this.tok.type === tt.incDec;
    node.operator = this.tok.value;
    node.prefix = true;
    this.next();
    node.argument = this.parseMaybeUnary(noIn);
    if (update) node.argument = this.checkLVal(node.argument);
    return this.finishNode(node, update ? "UpdateExpression" : "UnaryExpression");
  } else if (this.tok.type === tt.ellipsis) {
    var node = this.startNode();
    this.next();
    node.argument = this.parseMaybeUnary(noIn);
    return this.finishNode(node, "SpreadElement");
  }
  var start = this.storeCurrentPos();
  var expr = this.parseExprSubscripts();
  while (this.tok.type.postfix && !this.canInsertSemicolon()) {
    var node = this.startNodeAt(start);
    node.operator = this.tok.value;
    node.prefix = false;
    node.argument = this.checkLVal(expr);
    this.next();
    expr = this.finishNode(node, "UpdateExpression");
  }
  return expr;
};

lp.parseExprSubscripts = function () {
  var start = this.storeCurrentPos();
  return this.parseSubscripts(this.parseExprAtom(), start, false, this.curIndent, this.curLineStart);
};

lp.parseSubscripts = function (base, start, noCalls, startIndent, line) {
  for (;;) {
    if (this.curLineStart != line && this.curIndent <= startIndent && this.tokenStartsLine()) {
      if (this.tok.type == tt.dot && this.curIndent == startIndent) --startIndent;else return base;
    }

    if (this.eat(tt.dot)) {
      var node = this.startNodeAt(start);
      node.object = base;
      if (this.curLineStart != line && this.curIndent <= startIndent && this.tokenStartsLine()) node.property = this.dummyIdent();else node.property = this.parsePropertyAccessor() || this.dummyIdent();
      node.computed = false;
      base = this.finishNode(node, "MemberExpression");
    } else if (this.tok.type == tt.bracketL) {
      this.pushCx();
      this.next();
      var node = this.startNodeAt(start);
      node.object = base;
      node.property = this.parseExpression();
      node.computed = true;
      this.popCx();
      this.expect(tt.bracketR);
      base = this.finishNode(node, "MemberExpression");
    } else if (!noCalls && this.tok.type == tt.parenL) {
      var node = this.startNodeAt(start);
      node.callee = base;
      node.arguments = this.parseExprList(tt.parenR);
      base = this.finishNode(node, "CallExpression");
    } else if (this.tok.type == tt.backQuote) {
      var node = this.startNodeAt(start);
      node.tag = base;
      node.quasi = this.parseTemplate();
      base = this.finishNode(node, "TaggedTemplateExpression");
    } else {
      return base;
    }
  }
};

lp.parseExprAtom = function () {
  var node = undefined;
  switch (this.tok.type) {
    case tt._this:
    case tt._super:
      var type = this.tok.type === tt._this ? "ThisExpression" : "Super";
      node = this.startNode();
      this.next();
      return this.finishNode(node, type);

    case tt.name:
      var start = this.storeCurrentPos();
      var id = this.parseIdent();
      return this.eat(tt.arrow) ? this.parseArrowExpression(this.startNodeAt(start), [id]) : id;

    case tt.regexp:
      node = this.startNode();
      var val = this.tok.value;
      node.regex = { pattern: val.pattern, flags: val.flags };
      node.value = val.value;
      node.raw = this.input.slice(this.tok.start, this.tok.end);
      this.next();
      return this.finishNode(node, "Literal");

    case tt.num:case tt.string:
      node = this.startNode();
      node.value = this.tok.value;
      node.raw = this.input.slice(this.tok.start, this.tok.end);
      this.next();
      return this.finishNode(node, "Literal");

    case tt._null:case tt._true:case tt._false:
      node = this.startNode();
      node.value = this.tok.type === tt._null ? null : this.tok.type === tt._true;
      node.raw = this.tok.type.keyword;
      this.next();
      return this.finishNode(node, "Literal");

    case tt.parenL:
      var parenStart = this.storeCurrentPos();
      this.next();
      var inner = this.parseExpression();
      this.expect(tt.parenR);
      if (this.eat(tt.arrow)) {
        return this.parseArrowExpression(this.startNodeAt(parenStart), inner.expressions || (isDummy(inner) ? [] : [inner]));
      }
      if (this.options.preserveParens) {
        var par = this.startNodeAt(parenStart);
        par.expression = inner;
        inner = this.finishNode(par, "ParenthesizedExpression");
      }
      return inner;

    case tt.bracketL:
      node = this.startNode();
      node.elements = this.parseExprList(tt.bracketR, true);
      return this.finishNode(node, "ArrayExpression");

    case tt.braceL:
      return this.parseObj();

    case tt._class:
      return this.parseClass();

    case tt._function:
      node = this.startNode();
      this.next();
      return this.parseFunction(node, false);

    case tt._new:
      return this.parseNew();

    case tt._yield:
      node = this.startNode();
      this.next();
      if (this.semicolon() || this.canInsertSemicolon() || this.tok.type != tt.star && !this.tok.type.startsExpr) {
        node.delegate = false;
        node.argument = null;
      } else {
        node.delegate = this.eat(tt.star);
        node.argument = this.parseMaybeAssign();
      }
      return this.finishNode(node, "YieldExpression");

    case tt.backQuote:
      return this.parseTemplate();

    default:
      return this.dummyIdent();
  }
};

lp.parseNew = function () {
  var node = this.startNode(),
      startIndent = this.curIndent,
      line = this.curLineStart;
  var meta = this.parseIdent(true);
  if (this.options.ecmaVersion >= 6 && this.eat(tt.dot)) {
    node.meta = meta;
    node.property = this.parseIdent(true);
    return this.finishNode(node, "MetaProperty");
  }
  var start = this.storeCurrentPos();
  node.callee = this.parseSubscripts(this.parseExprAtom(), start, true, startIndent, line);
  if (this.tok.type == tt.parenL) {
    node.arguments = this.parseExprList(tt.parenR);
  } else {
    node.arguments = [];
  }
  return this.finishNode(node, "NewExpression");
};

lp.parseTemplateElement = function () {
  var elem = this.startNode();
  elem.value = {
    raw: this.input.slice(this.tok.start, this.tok.end),
    cooked: this.tok.value
  };
  this.next();
  elem.tail = this.tok.type === tt.backQuote;
  return this.finishNode(elem, "TemplateElement");
};

lp.parseTemplate = function () {
  var node = this.startNode();
  this.next();
  node.expressions = [];
  var curElt = this.parseTemplateElement();
  node.quasis = [curElt];
  while (!curElt.tail) {
    this.next();
    node.expressions.push(this.parseExpression());
    if (this.expect(tt.braceR)) {
      curElt = this.parseTemplateElement();
    } else {
      curElt = this.startNode();
      curElt.value = { cooked: "", raw: "" };
      curElt.tail = true;
    }
    node.quasis.push(curElt);
  }
  this.expect(tt.backQuote);
  return this.finishNode(node, "TemplateLiteral");
};

lp.parseObj = function () {
  var node = this.startNode();
  node.properties = [];
  this.pushCx();
  var indent = this.curIndent + 1,
      line = this.curLineStart;
  this.eat(tt.braceL);
  if (this.curIndent + 1 < indent) {
    indent = this.curIndent;line = this.curLineStart;
  }
  while (!this.closes(tt.braceR, indent, line)) {
    var prop = this.startNode(),
        isGenerator = undefined,
        start = undefined;
    if (this.options.ecmaVersion >= 6) {
      start = this.storeCurrentPos();
      prop.method = false;
      prop.shorthand = false;
      isGenerator = this.eat(tt.star);
    }
    this.parsePropertyName(prop);
    if (isDummy(prop.key)) {
      if (isDummy(this.parseMaybeAssign())) this.next();this.eat(tt.comma);continue;
    }
    if (this.eat(tt.colon)) {
      prop.kind = "init";
      prop.value = this.parseMaybeAssign();
    } else if (this.options.ecmaVersion >= 6 && (this.tok.type === tt.parenL || this.tok.type === tt.braceL)) {
      prop.kind = "init";
      prop.method = true;
      prop.value = this.parseMethod(isGenerator);
    } else if (this.options.ecmaVersion >= 5 && prop.key.type === "Identifier" && !prop.computed && (prop.key.name === "get" || prop.key.name === "set") && (this.tok.type != tt.comma && this.tok.type != tt.braceR)) {
      prop.kind = prop.key.name;
      this.parsePropertyName(prop);
      prop.value = this.parseMethod(false);
    } else {
      prop.kind = "init";
      if (this.options.ecmaVersion >= 6) {
        if (this.eat(tt.eq)) {
          var assign = this.startNodeAt(start);
          assign.operator = "=";
          assign.left = prop.key;
          assign.right = this.parseMaybeAssign();
          prop.value = this.finishNode(assign, "AssignmentExpression");
        } else {
          prop.value = prop.key;
        }
      } else {
        prop.value = this.dummyIdent();
      }
      prop.shorthand = true;
    }
    node.properties.push(this.finishNode(prop, "Property"));
    this.eat(tt.comma);
  }
  this.popCx();
  if (!this.eat(tt.braceR)) {
    // If there is no closing brace, make the node span to the start
    // of the next token (this is useful for Tern)
    this.last.end = this.tok.start;
    if (this.options.locations) this.last.loc.end = this.tok.loc.start;
  }
  return this.finishNode(node, "ObjectExpression");
};

lp.parsePropertyName = function (prop) {
  if (this.options.ecmaVersion >= 6) {
    if (this.eat(tt.bracketL)) {
      prop.computed = true;
      prop.key = this.parseExpression();
      this.expect(tt.bracketR);
      return;
    } else {
      prop.computed = false;
    }
  }
  var key = this.tok.type === tt.num || this.tok.type === tt.string ? this.parseExprAtom() : this.parseIdent();
  prop.key = key || this.dummyIdent();
};

lp.parsePropertyAccessor = function () {
  if (this.tok.type === tt.name || this.tok.type.keyword) return this.parseIdent();
};

lp.parseIdent = function () {
  var name = this.tok.type === tt.name ? this.tok.value : this.tok.type.keyword;
  if (!name) return this.dummyIdent();
  var node = this.startNode();
  this.next();
  node.name = name;
  return this.finishNode(node, "Identifier");
};

lp.initFunction = function (node) {
  node.id = null;
  node.params = [];
  if (this.options.ecmaVersion >= 6) {
    node.generator = false;
    node.expression = false;
  }
};

// Convert existing expression atom to assignable pattern
// if possible.

lp.toAssignable = function (node, binding) {
  if (this.options.ecmaVersion >= 6 && node) {
    switch (node.type) {
      case "ObjectExpression":
        node.type = "ObjectPattern";
        var props = node.properties;
        for (var i = 0; i < props.length; i++) {
          this.toAssignable(props[i].value, binding);
        }break;

      case "ArrayExpression":
        node.type = "ArrayPattern";
        this.toAssignableList(node.elements, binding);
        break;

      case "SpreadElement":
        node.type = "RestElement";
        node.argument = this.toAssignable(node.argument, binding);
        break;

      case "AssignmentExpression":
        node.type = "AssignmentPattern";
        break;
    }
  }
  return this.checkLVal(node, binding);
};

lp.toAssignableList = function (exprList, binding) {
  for (var i = 0; i < exprList.length; i++) {
    exprList[i] = this.toAssignable(exprList[i], binding);
  }return exprList;
};

lp.parseFunctionParams = function (params) {
  params = this.parseExprList(tt.parenR);
  return this.toAssignableList(params, true);
};

lp.parseMethod = function (isGenerator) {
  var node = this.startNode();
  this.initFunction(node);
  node.params = this.parseFunctionParams();
  node.generator = isGenerator || false;
  node.expression = this.options.ecmaVersion >= 6 && this.tok.type !== tt.braceL;
  node.body = node.expression ? this.parseMaybeAssign() : this.parseBlock();
  return this.finishNode(node, "FunctionExpression");
};

lp.parseArrowExpression = function (node, params) {
  this.initFunction(node);
  node.params = this.toAssignableList(params, true);
  node.expression = this.tok.type !== tt.braceL;
  node.body = node.expression ? this.parseMaybeAssign() : this.parseBlock();
  return this.finishNode(node, "ArrowFunctionExpression");
};

lp.parseExprList = function (close, allowEmpty) {
  this.pushCx();
  var indent = this.curIndent,
      line = this.curLineStart,
      elts = [];
  this.next(); // Opening bracket
  while (!this.closes(close, indent + 1, line)) {
    if (this.eat(tt.comma)) {
      elts.push(allowEmpty ? null : this.dummyIdent());
      continue;
    }
    var elt = this.parseMaybeAssign();
    if (isDummy(elt)) {
      if (this.closes(close, indent, line)) break;
      this.next();
    } else {
      elts.push(elt);
    }
    this.eat(tt.comma);
  }
  this.popCx();
  if (!this.eat(close)) {
    // If there is no closing brace, make the node span to the start
    // of the next token (this is useful for Tern)
    this.last.end = this.tok.start;
    if (this.options.locations) this.last.loc.end = this.tok.loc.start;
  }
  return elts;
};

},{"..":2,"./parseutil":4,"./state":5}],4:[function(_dereq_,module,exports){
"use strict";

exports.isDummy = isDummy;
exports.__esModule = true;

var LooseParser = _dereq_("./state").LooseParser;

var _ = _dereq_("..");

var Node = _.Node;
var SourceLocation = _.SourceLocation;
var lineBreak = _.lineBreak;
var isNewLine = _.isNewLine;
var tt = _.tokTypes;

var lp = LooseParser.prototype;

lp.startNode = function () {
  var node = new Node();
  node.start = this.tok.start;
  if (this.options.locations) node.loc = new SourceLocation(this.toks, this.tok.loc.start);
  if (this.options.directSourceFile) node.sourceFile = this.options.directSourceFile;
  if (this.options.ranges) node.range = [this.tok.start, 0];
  return node;
};

lp.storeCurrentPos = function () {
  return this.options.locations ? [this.tok.start, this.tok.loc.start] : this.tok.start;
};

lp.startNodeAt = function (pos) {
  var node = new Node();
  if (this.options.locations) {
    node.start = pos[0];
    node.loc = new SourceLocation(this.toks, pos[1]);
    pos = pos[0];
  } else {
    node.start = pos;
  }
  if (this.options.directSourceFile) node.sourceFile = this.options.directSourceFile;
  if (this.options.ranges) node.range = [pos, 0];
  return node;
};

lp.finishNode = function (node, type) {
  node.type = type;
  node.end = this.last.end;
  if (this.options.locations) node.loc.end = this.last.loc.end;
  if (this.options.ranges) node.range[1] = this.last.end;
  return node;
};

lp.dummyIdent = function () {
  var dummy = this.startNode();
  dummy.name = "✖";
  return this.finishNode(dummy, "Identifier");
};

function isDummy(node) {
  return node.name == "✖";
}

lp.eat = function (type) {
  if (this.tok.type === type) {
    this.next();
    return true;
  } else {
    return false;
  }
};

lp.isContextual = function (name) {
  return this.tok.type === tt.name && this.tok.value === name;
};

lp.eatContextual = function (name) {
  return this.tok.value === name && this.eat(tt.name);
};

lp.canInsertSemicolon = function () {
  return this.tok.type === tt.eof || this.tok.type === tt.braceR || lineBreak.test(this.input.slice(this.last.end, this.tok.start));
};

lp.semicolon = function () {
  return this.eat(tt.semi);
};

lp.expect = function (type) {
  if (this.eat(type)) return true;
  for (var i = 1; i <= 2; i++) {
    if (this.lookAhead(i).type == type) {
      for (var j = 0; j < i; j++) {
        this.next();
      }return true;
    }
  }
};

lp.pushCx = function () {
  this.context.push(this.curIndent);
};
lp.popCx = function () {
  this.curIndent = this.context.pop();
};

lp.lineEnd = function (pos) {
  while (pos < this.input.length && !isNewLine(this.input.charCodeAt(pos))) ++pos;
  return pos;
};

lp.indentationAfter = function (pos) {
  for (var count = 0;; ++pos) {
    var ch = this.input.charCodeAt(pos);
    if (ch === 32) ++count;else if (ch === 9) count += this.options.tabSize;else return count;
  }
};

lp.closes = function (closeTok, indent, line, blockHeuristic) {
  if (this.tok.type === closeTok || this.tok.type === tt.eof) return true;
  return line != this.curLineStart && this.curIndent < indent && this.tokenStartsLine() && (!blockHeuristic || this.nextLineStart >= this.input.length || this.indentationAfter(this.nextLineStart) < indent);
};

lp.tokenStartsLine = function () {
  for (var p = this.tok.start - 1; p >= this.curLineStart; --p) {
    var ch = this.input.charCodeAt(p);
    if (ch !== 9 && ch !== 32) return false;
  }
  return true;
};

},{"..":2,"./state":5}],5:[function(_dereq_,module,exports){
"use strict";

exports.LooseParser = LooseParser;
exports.__esModule = true;

var _ = _dereq_("..");

var tokenizer = _.tokenizer;
var SourceLocation = _.SourceLocation;
var tt = _.tokTypes;

function LooseParser(input, options) {
  this.toks = tokenizer(input, options);
  this.options = this.toks.options;
  this.input = this.toks.input;
  this.tok = this.last = { type: tt.eof, start: 0, end: 0 };
  if (this.options.locations) {
    var here = this.toks.curPosition();
    this.tok.loc = new SourceLocation(this.toks, here, here);
  }
  this.ahead = []; // Tokens ahead
  this.context = []; // Indentation contexted
  this.curIndent = 0;
  this.curLineStart = 0;
  this.nextLineStart = this.lineEnd(this.curLineStart) + 1;
}

},{"..":2}],6:[function(_dereq_,module,exports){
"use strict";

var LooseParser = _dereq_("./state").LooseParser;

var isDummy = _dereq_("./parseutil").isDummy;

var _ = _dereq_("..");

var getLineInfo = _.getLineInfo;
var tt = _.tokTypes;

var lp = LooseParser.prototype;

lp.parseTopLevel = function () {
  var node = this.startNodeAt(this.options.locations ? [0, getLineInfo(this.input, 0)] : 0);
  node.body = [];
  while (this.tok.type !== tt.eof) node.body.push(this.parseStatement());
  this.last = this.tok;
  if (this.options.ecmaVersion >= 6) {
    node.sourceType = this.options.sourceType;
  }
  return this.finishNode(node, "Program");
};

lp.parseStatement = function () {
  var starttype = this.tok.type,
      node = this.startNode();

  switch (starttype) {
    case tt._break:case tt._continue:
      this.next();
      var isBreak = starttype === tt._break;
      if (this.semicolon() || this.canInsertSemicolon()) {
        node.label = null;
      } else {
        node.label = this.tok.type === tt.name ? this.parseIdent() : null;
        this.semicolon();
      }
      return this.finishNode(node, isBreak ? "BreakStatement" : "ContinueStatement");

    case tt._debugger:
      this.next();
      this.semicolon();
      return this.finishNode(node, "DebuggerStatement");

    case tt._do:
      this.next();
      node.body = this.parseStatement();
      node.test = this.eat(tt._while) ? this.parseParenExpression() : this.dummyIdent();
      this.semicolon();
      return this.finishNode(node, "DoWhileStatement");

    case tt._for:
      this.next();
      this.pushCx();
      this.expect(tt.parenL);
      if (this.tok.type === tt.semi) return this.parseFor(node, null);
      if (this.tok.type === tt._var || this.tok.type === tt._let || this.tok.type === tt._const) {
        var _init = this.parseVar(true);
        if (_init.declarations.length === 1 && (this.tok.type === tt._in || this.isContextual("of"))) {
          return this.parseForIn(node, _init);
        }
        return this.parseFor(node, _init);
      }
      var init = this.parseExpression(true);
      if (this.tok.type === tt._in || this.isContextual("of")) return this.parseForIn(node, this.toAssignable(init));
      return this.parseFor(node, init);

    case tt._function:
      this.next();
      return this.parseFunction(node, true);

    case tt._if:
      this.next();
      node.test = this.parseParenExpression();
      node.consequent = this.parseStatement();
      node.alternate = this.eat(tt._else) ? this.parseStatement() : null;
      return this.finishNode(node, "IfStatement");

    case tt._return:
      this.next();
      if (this.eat(tt.semi) || this.canInsertSemicolon()) node.argument = null;else {
        node.argument = this.parseExpression();this.semicolon();
      }
      return this.finishNode(node, "ReturnStatement");

    case tt._switch:
      var blockIndent = this.curIndent,
          line = this.curLineStart;
      this.next();
      node.discriminant = this.parseParenExpression();
      node.cases = [];
      this.pushCx();
      this.expect(tt.braceL);

      var cur = undefined;
      while (!this.closes(tt.braceR, blockIndent, line, true)) {
        if (this.tok.type === tt._case || this.tok.type === tt._default) {
          var isCase = this.tok.type === tt._case;
          if (cur) this.finishNode(cur, "SwitchCase");
          node.cases.push(cur = this.startNode());
          cur.consequent = [];
          this.next();
          if (isCase) cur.test = this.parseExpression();else cur.test = null;
          this.expect(tt.colon);
        } else {
          if (!cur) {
            node.cases.push(cur = this.startNode());
            cur.consequent = [];
            cur.test = null;
          }
          cur.consequent.push(this.parseStatement());
        }
      }
      if (cur) this.finishNode(cur, "SwitchCase");
      this.popCx();
      this.eat(tt.braceR);
      return this.finishNode(node, "SwitchStatement");

    case tt._throw:
      this.next();
      node.argument = this.parseExpression();
      this.semicolon();
      return this.finishNode(node, "ThrowStatement");

    case tt._try:
      this.next();
      node.block = this.parseBlock();
      node.handler = null;
      if (this.tok.type === tt._catch) {
        var clause = this.startNode();
        this.next();
        this.expect(tt.parenL);
        clause.param = this.toAssignable(this.parseExprAtom(), true);
        this.expect(tt.parenR);
        clause.guard = null;
        clause.body = this.parseBlock();
        node.handler = this.finishNode(clause, "CatchClause");
      }
      node.finalizer = this.eat(tt._finally) ? this.parseBlock() : null;
      if (!node.handler && !node.finalizer) return node.block;
      return this.finishNode(node, "TryStatement");

    case tt._var:
    case tt._let:
    case tt._const:
      return this.parseVar();

    case tt._while:
      this.next();
      node.test = this.parseParenExpression();
      node.body = this.parseStatement();
      return this.finishNode(node, "WhileStatement");

    case tt._with:
      this.next();
      node.object = this.parseParenExpression();
      node.body = this.parseStatement();
      return this.finishNode(node, "WithStatement");

    case tt.braceL:
      return this.parseBlock();

    case tt.semi:
      this.next();
      return this.finishNode(node, "EmptyStatement");

    case tt._class:
      return this.parseClass(true);

    case tt._import:
      return this.parseImport();

    case tt._export:
      return this.parseExport();

    default:
      var expr = this.parseExpression();
      if (isDummy(expr)) {
        this.next();
        if (this.tok.type === tt.eof) return this.finishNode(node, "EmptyStatement");
        return this.parseStatement();
      } else if (starttype === tt.name && expr.type === "Identifier" && this.eat(tt.colon)) {
        node.body = this.parseStatement();
        node.label = expr;
        return this.finishNode(node, "LabeledStatement");
      } else {
        node.expression = expr;
        this.semicolon();
        return this.finishNode(node, "ExpressionStatement");
      }
  }
};

lp.parseBlock = function () {
  var node = this.startNode();
  this.pushCx();
  this.expect(tt.braceL);
  var blockIndent = this.curIndent,
      line = this.curLineStart;
  node.body = [];
  while (!this.closes(tt.braceR, blockIndent, line, true)) node.body.push(this.parseStatement());
  this.popCx();
  this.eat(tt.braceR);
  return this.finishNode(node, "BlockStatement");
};

lp.parseFor = function (node, init) {
  node.init = init;
  node.test = node.update = null;
  if (this.eat(tt.semi) && this.tok.type !== tt.semi) node.test = this.parseExpression();
  if (this.eat(tt.semi) && this.tok.type !== tt.parenR) node.update = this.parseExpression();
  this.popCx();
  this.expect(tt.parenR);
  node.body = this.parseStatement();
  return this.finishNode(node, "ForStatement");
};

lp.parseForIn = function (node, init) {
  var type = this.tok.type === tt._in ? "ForInStatement" : "ForOfStatement";
  this.next();
  node.left = init;
  node.right = this.parseExpression();
  this.popCx();
  this.expect(tt.parenR);
  node.body = this.parseStatement();
  return this.finishNode(node, type);
};

lp.parseVar = function (noIn) {
  var node = this.startNode();
  node.kind = this.tok.type.keyword;
  this.next();
  node.declarations = [];
  do {
    var decl = this.startNode();
    decl.id = this.options.ecmaVersion >= 6 ? this.toAssignable(this.parseExprAtom(), true) : this.parseIdent();
    decl.init = this.eat(tt.eq) ? this.parseMaybeAssign(noIn) : null;
    node.declarations.push(this.finishNode(decl, "VariableDeclarator"));
  } while (this.eat(tt.comma));
  if (!node.declarations.length) {
    var decl = this.startNode();
    decl.id = this.dummyIdent();
    node.declarations.push(this.finishNode(decl, "VariableDeclarator"));
  }
  if (!noIn) this.semicolon();
  return this.finishNode(node, "VariableDeclaration");
};

lp.parseClass = function (isStatement) {
  var node = this.startNode();
  this.next();
  if (this.tok.type === tt.name) node.id = this.parseIdent();else if (isStatement) node.id = this.dummyIdent();else node.id = null;
  node.superClass = this.eat(tt._extends) ? this.parseExpression() : null;
  node.body = this.startNode();
  node.body.body = [];
  this.pushCx();
  var indent = this.curIndent + 1,
      line = this.curLineStart;
  this.eat(tt.braceL);
  if (this.curIndent + 1 < indent) {
    indent = this.curIndent;line = this.curLineStart;
  }
  while (!this.closes(tt.braceR, indent, line)) {
    if (this.semicolon()) continue;
    var method = this.startNode(),
        isGenerator = undefined;
    if (this.options.ecmaVersion >= 6) {
      method["static"] = false;
      isGenerator = this.eat(tt.star);
    }
    this.parsePropertyName(method);
    if (isDummy(method.key)) {
      if (isDummy(this.parseMaybeAssign())) this.next();this.eat(tt.comma);continue;
    }
    if (method.key.type === "Identifier" && !method.computed && method.key.name === "static" && (this.tok.type != tt.parenL && this.tok.type != tt.braceL)) {
      method["static"] = true;
      isGenerator = this.eat(tt.star);
      this.parsePropertyName(method);
    } else {
      method["static"] = false;
    }
    if (this.options.ecmaVersion >= 5 && method.key.type === "Identifier" && !method.computed && (method.key.name === "get" || method.key.name === "set") && this.tok.type !== tt.parenL && this.tok.type !== tt.braceL) {
      method.kind = method.key.name;
      this.parsePropertyName(method);
      method.value = this.parseMethod(false);
    } else {
      if (!method.computed && !method["static"] && !isGenerator && (method.key.type === "Identifier" && method.key.name === "constructor" || method.key.type === "Literal" && method.key.value === "constructor")) {
        method.kind = "constructor";
      } else {
        method.kind = "method";
      }
      method.value = this.parseMethod(isGenerator);
    }
    node.body.body.push(this.finishNode(method, "MethodDefinition"));
  }
  this.popCx();
  if (!this.eat(tt.braceR)) {
    // If there is no closing brace, make the node span to the start
    // of the next token (this is useful for Tern)
    this.last.end = this.tok.start;
    if (this.options.locations) this.last.loc.end = this.tok.loc.start;
  }
  this.semicolon();
  this.finishNode(node.body, "ClassBody");
  return this.finishNode(node, isStatement ? "ClassDeclaration" : "ClassExpression");
};

lp.parseFunction = function (node, isStatement) {
  this.initFunction(node);
  if (this.options.ecmaVersion >= 6) {
    node.generator = this.eat(tt.star);
  }
  if (this.tok.type === tt.name) node.id = this.parseIdent();else if (isStatement) node.id = this.dummyIdent();
  node.params = this.parseFunctionParams();
  node.body = this.parseBlock();
  return this.finishNode(node, isStatement ? "FunctionDeclaration" : "FunctionExpression");
};

lp.parseExport = function () {
  var node = this.startNode();
  this.next();
  if (this.eat(tt.star)) {
    node.source = this.eatContextual("from") ? this.parseExprAtom() : null;
    return this.finishNode(node, "ExportAllDeclaration");
  }
  if (this.eat(tt._default)) {
    var expr = this.parseMaybeAssign();
    if (expr.id) {
      switch (expr.type) {
        case "FunctionExpression":
          expr.type = "FunctionDeclaration";break;
        case "ClassExpression":
          expr.type = "ClassDeclaration";break;
      }
    }
    node.declaration = expr;
    this.semicolon();
    return this.finishNode(node, "ExportDefaultDeclaration");
  }
  if (this.tok.type.keyword) {
    node.declaration = this.parseStatement();
    node.specifiers = [];
    node.source = null;
  } else {
    node.declaration = null;
    node.specifiers = this.parseExportSpecifierList();
    node.source = this.eatContextual("from") ? this.parseExprAtom() : null;
    this.semicolon();
  }
  return this.finishNode(node, "ExportNamedDeclaration");
};

lp.parseImport = function () {
  var node = this.startNode();
  this.next();
  if (this.tok.type === tt.string) {
    node.specifiers = [];
    node.source = this.parseExprAtom();
    node.kind = "";
  } else {
    var elt = undefined;
    if (this.tok.type === tt.name && this.tok.value !== "from") {
      elt = this.startNode();
      elt.local = this.parseIdent();
      this.finishNode(elt, "ImportDefaultSpecifier");
      this.eat(tt.comma);
    }
    node.specifiers = this.parseImportSpecifierList();
    node.source = this.eatContextual("from") ? this.parseExprAtom() : null;
    if (elt) node.specifiers.unshift(elt);
  }
  this.semicolon();
  return this.finishNode(node, "ImportDeclaration");
};

lp.parseImportSpecifierList = function () {
  var elts = [];
  if (this.tok.type === tt.star) {
    var elt = this.startNode();
    this.next();
    if (this.eatContextual("as")) elt.local = this.parseIdent();
    elts.push(this.finishNode(elt, "ImportNamespaceSpecifier"));
  } else {
    var indent = this.curIndent,
        line = this.curLineStart,
        continuedLine = this.nextLineStart;
    this.pushCx();
    this.eat(tt.braceL);
    if (this.curLineStart > continuedLine) continuedLine = this.curLineStart;
    while (!this.closes(tt.braceR, indent + (this.curLineStart <= continuedLine ? 1 : 0), line)) {
      var elt = this.startNode();
      if (this.eat(tt.star)) {
        if (this.eatContextual("as")) elt.local = this.parseIdent();
        this.finishNode(elt, "ImportNamespaceSpecifier");
      } else {
        if (this.isContextual("from")) break;
        elt.imported = this.parseIdent();
        elt.local = this.eatContextual("as") ? this.parseIdent() : elt.imported;
        this.finishNode(elt, "ImportSpecifier");
      }
      elts.push(elt);
      this.eat(tt.comma);
    }
    this.eat(tt.braceR);
    this.popCx();
  }
  return elts;
};

lp.parseExportSpecifierList = function () {
  var elts = [];
  var indent = this.curIndent,
      line = this.curLineStart,
      continuedLine = this.nextLineStart;
  this.pushCx();
  this.eat(tt.braceL);
  if (this.curLineStart > continuedLine) continuedLine = this.curLineStart;
  while (!this.closes(tt.braceR, indent + (this.curLineStart <= continuedLine ? 1 : 0), line)) {
    if (this.isContextual("from")) break;
    var elt = this.startNode();
    elt.local = this.parseIdent();
    elt.exported = this.eatContextual("as") ? this.parseIdent() : elt.local;
    this.finishNode(elt, "ExportSpecifier");
    elts.push(elt);
    this.eat(tt.comma);
  }
  this.eat(tt.braceR);
  this.popCx();
  return elts;
};

},{"..":2,"./parseutil":4,"./state":5}],7:[function(_dereq_,module,exports){
"use strict";

var _ = _dereq_("..");

var tt = _.tokTypes;
var Token = _.Token;
var isNewLine = _.isNewLine;
var SourceLocation = _.SourceLocation;
var getLineInfo = _.getLineInfo;
var lineBreakG = _.lineBreakG;

var LooseParser = _dereq_("./state").LooseParser;

var lp = LooseParser.prototype;

function isSpace(ch) {
  return ch < 14 && ch > 8 || ch === 32 || ch === 160 || isNewLine(ch);
}

lp.next = function () {
  this.last = this.tok;
  if (this.ahead.length) this.tok = this.ahead.shift();else this.tok = this.readToken();

  if (this.tok.start >= this.nextLineStart) {
    while (this.tok.start >= this.nextLineStart) {
      this.curLineStart = this.nextLineStart;
      this.nextLineStart = this.lineEnd(this.curLineStart) + 1;
    }
    this.curIndent = this.indentationAfter(this.curLineStart);
  }
};

lp.readToken = function () {
  for (;;) {
    try {
      this.toks.next();
      if (this.toks.type === tt.dot && this.input.substr(this.toks.end, 1) === "." && this.options.ecmaVersion >= 6) {
        this.toks.end++;
        this.toks.type = tt.ellipsis;
      }
      return new Token(this.toks);
    } catch (e) {
      if (!(e instanceof SyntaxError)) throw e;

      // Try to skip some text, based on the error message, and then continue
      var msg = e.message,
          pos = e.raisedAt,
          replace = true;
      if (/unterminated/i.test(msg)) {
        pos = this.lineEnd(e.pos + 1);
        if (/string/.test(msg)) {
          replace = { start: e.pos, end: pos, type: tt.string, value: this.input.slice(e.pos + 1, pos) };
        } else if (/regular expr/i.test(msg)) {
          var re = this.input.slice(e.pos, pos);
          try {
            re = new RegExp(re);
          } catch (e) {}
          replace = { start: e.pos, end: pos, type: tt.regexp, value: re };
        } else if (/template/.test(msg)) {
          replace = { start: e.pos, end: pos,
            type: tt.template,
            value: this.input.slice(e.pos, pos) };
        } else {
          replace = false;
        }
      } else if (/invalid (unicode|regexp|number)|expecting unicode|octal literal|is reserved|directly after number|expected number in radix/i.test(msg)) {
        while (pos < this.input.length && !isSpace(this.input.charCodeAt(pos))) ++pos;
      } else if (/character escape|expected hexadecimal/i.test(msg)) {
        while (pos < this.input.length) {
          var ch = this.input.charCodeAt(pos++);
          if (ch === 34 || ch === 39 || isNewLine(ch)) break;
        }
      } else if (/unexpected character/i.test(msg)) {
        pos++;
        replace = false;
      } else if (/regular expression/i.test(msg)) {
        replace = true;
      } else {
        throw e;
      }
      this.resetTo(pos);
      if (replace === true) replace = { start: pos, end: pos, type: tt.name, value: "✖" };
      if (replace) {
        if (this.options.locations) replace.loc = new SourceLocation(this.toks, getLineInfo(this.input, replace.start), getLineInfo(this.input, replace.end));
        return replace;
      }
    }
  }
};

lp.resetTo = function (pos) {
  this.toks.pos = pos;
  var ch = this.input.charAt(pos - 1);
  this.toks.exprAllowed = !ch || /[\[\{\(,;:?\/*=+\-~!|&%^<>]/.test(ch) || /[enwfd]/.test(ch) && /\b(keywords|case|else|return|throw|new|in|(instance|type)of|delete|void)$/.test(this.input.slice(pos - 10, pos));

  if (this.options.locations) {
    this.toks.curLine = 1;
    this.toks.lineStart = lineBreakG.lastIndex = 0;
    var match = undefined;
    while ((match = lineBreakG.exec(this.input)) && match.index < pos) {
      ++this.toks.curLine;
      this.toks.lineStart = match.index + match[0].length;
    }
  }
};

lp.lookAhead = function (n) {
  while (n > this.ahead.length) this.ahead.push(this.readToken());
  return this.ahead[n - 1];
};

},{"..":2,"./state":5}]},{},[1])(1)
});