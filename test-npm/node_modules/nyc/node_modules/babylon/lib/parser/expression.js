"use strict";

var _create = require("babel-runtime/core-js/object/create");

var _create2 = _interopRequireDefault(_create);

var _getIterator2 = require("babel-runtime/core-js/get-iterator");

var _getIterator3 = _interopRequireDefault(_getIterator2);

var _types = require("../tokenizer/types");

var _index = require("./index");

var _index2 = _interopRequireDefault(_index);

var _identifier = require("../util/identifier");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

var pp = _index2.default.prototype;

// Check if property name clashes with already added.
// Object/class getters and setters are not allowed to clash —
// either with each other or with an init property — and in
// strict mode, init properties are also not allowed to be repeated.

/* eslint indent: 0 */
/* eslint max-len: 0 */

// A recursive descent parser operates by defining functions for all
// syntactic elements, and recursively calling those, each function
// advancing the input stream and returning an AST node. Precedence
// of constructs (for example, the fact that `!x[1]` means `!(x[1])`
// instead of `(!x)[1]` is handled by the fact that the parser
// function that parses unary prefix operators is called first, and
// in turn calls the function that parses `[]` subscripts — that
// way, it'll receive the node for `x[1]` already parsed, and wraps
// *that* in the unary operator node.
//
// Acorn uses an [operator precedence parser][opp] to handle binary
// operator precedence, because it is much more compact than using
// the technique outlined above, which uses different, nesting
// functions to specify precedence, for all of the ten binary
// precedence levels that JavaScript defines.
//
// [opp]: http://en.wikipedia.org/wiki/Operator-precedence_parser

pp.checkPropClash = function (prop, propHash) {
  if (prop.computed) return;

  var key = prop.key;
  var name = void 0;
  switch (key.type) {
    case "Identifier":
      name = key.name;
      break;

    case "StringLiteral":
    case "NumericLiteral":
      name = String(key.value);
      break;

    default:
      return;
  }

  if (name === "__proto__" && prop.kind === "init") {
    if (propHash.proto) this.raise(key.start, "Redefinition of __proto__ property");
    propHash.proto = true;
  }
};

// ### Expression parsing

// These nest, from the most general expression type at the top to
// 'atomic', nondivisible expression types at the bottom. Most of
// the functions will simply let the function (s) below them parse,
// and, *if* the syntactic construct they handle is present, wrap
// the AST node that the inner parser gave them in another node.

// Parse a full expression. The optional arguments are used to
// forbid the `in` operator (in for loops initalization expressions)
// and provide reference for storing '=' operator inside shorthand
// property assignment in contexts where both object expression
// and object pattern might appear (so it's possible to raise
// delayed syntax error at correct position).

pp.parseExpression = function (noIn, refShorthandDefaultPos) {
  var startPos = this.state.start,
      startLoc = this.state.startLoc;
  var expr = this.parseMaybeAssign(noIn, refShorthandDefaultPos);
  if (this.match(_types.types.comma)) {
    var node = this.startNodeAt(startPos, startLoc);
    node.expressions = [expr];
    while (this.eat(_types.types.comma)) {
      node.expressions.push(this.parseMaybeAssign(noIn, refShorthandDefaultPos));
    }
    this.toReferencedList(node.expressions);
    return this.finishNode(node, "SequenceExpression");
  }
  return expr;
};

// Parse an assignment expression. This includes applications of
// operators like `+=`.

pp.parseMaybeAssign = function (noIn, refShorthandDefaultPos, afterLeftParse, refNeedsArrowPos) {
  if (this.match(_types.types._yield) && this.state.inGenerator) {
    return this.parseYield();
  }

  var failOnShorthandAssign = void 0;
  if (refShorthandDefaultPos) {
    failOnShorthandAssign = false;
  } else {
    refShorthandDefaultPos = { start: 0 };
    failOnShorthandAssign = true;
  }

  var startPos = this.state.start;
  var startLoc = this.state.startLoc;

  if (this.match(_types.types.parenL) || this.match(_types.types.name)) {
    this.state.potentialArrowAt = this.state.start;
  }

  var left = this.parseMaybeConditional(noIn, refShorthandDefaultPos, refNeedsArrowPos);
  if (afterLeftParse) left = afterLeftParse.call(this, left, startPos, startLoc);
  if (this.state.type.isAssign) {
    var node = this.startNodeAt(startPos, startLoc);
    node.operator = this.state.value;
    node.left = this.match(_types.types.eq) ? this.toAssignable(left) : left;
    refShorthandDefaultPos.start = 0; // reset because shorthand default was used correctly

    this.checkLVal(left);

    if (left.extra && left.extra.parenthesized) {
      var errorMsg = void 0;
      if (left.type === "ObjectPattern") {
        errorMsg = "`({a}) = 0` use `({a} = 0)`";
      } else if (left.type === "ArrayPattern") {
        errorMsg = "`([a]) = 0` use `([a] = 0)`";
      }
      if (errorMsg) {
        this.raise(left.start, "You're trying to assign to a parenthesized expression, eg. instead of " + errorMsg);
      }
    }

    this.next();
    node.right = this.parseMaybeAssign(noIn);
    return this.finishNode(node, "AssignmentExpression");
  } else if (failOnShorthandAssign && refShorthandDefaultPos.start) {
    this.unexpected(refShorthandDefaultPos.start);
  }

  return left;
};

// Parse a ternary conditional (`?:`) operator.

pp.parseMaybeConditional = function (noIn, refShorthandDefaultPos, refNeedsArrowPos) {
  var startPos = this.state.start,
      startLoc = this.state.startLoc;
  var expr = this.parseExprOps(noIn, refShorthandDefaultPos);
  if (refShorthandDefaultPos && refShorthandDefaultPos.start) return expr;

  return this.parseConditional(expr, noIn, startPos, startLoc, refNeedsArrowPos);
};

pp.parseConditional = function (expr, noIn, startPos, startLoc) {
  if (this.eat(_types.types.question)) {
    var node = this.startNodeAt(startPos, startLoc);
    node.test = expr;
    node.consequent = this.parseMaybeAssign();
    this.expect(_types.types.colon);
    node.alternate = this.parseMaybeAssign(noIn);
    return this.finishNode(node, "ConditionalExpression");
  }
  return expr;
};

// Start the precedence parser.

pp.parseExprOps = function (noIn, refShorthandDefaultPos) {
  var startPos = this.state.start,
      startLoc = this.state.startLoc;
  var expr = this.parseMaybeUnary(refShorthandDefaultPos);
  if (refShorthandDefaultPos && refShorthandDefaultPos.start) {
    return expr;
  } else {
    return this.parseExprOp(expr, startPos, startLoc, -1, noIn);
  }
};

// Parse binary operators with the operator precedence parsing
// algorithm. `left` is the left-hand side of the operator.
// `minPrec` provides context that allows the function to stop and
// defer further parser to one of its callers when it encounters an
// operator that has a lower precedence than the set it is parsing.

pp.parseExprOp = function (left, leftStartPos, leftStartLoc, minPrec, noIn) {
  var prec = this.state.type.binop;
  if (prec != null && (!noIn || !this.match(_types.types._in))) {
    if (prec > minPrec) {
      var node = this.startNodeAt(leftStartPos, leftStartLoc);
      node.left = left;
      node.operator = this.state.value;

      if (node.operator === "**" && left.type === "UnaryExpression" && left.extra && !left.extra.parenthesizedArgument) {
        this.raise(left.argument.start, "Illegal expression. Wrap left hand side or entire exponentiation in parentheses.");
      }

      var op = this.state.type;
      this.next();

      var startPos = this.state.start;
      var startLoc = this.state.startLoc;
      node.right = this.parseExprOp(this.parseMaybeUnary(), startPos, startLoc, op.rightAssociative ? prec - 1 : prec, noIn);

      this.finishNode(node, op === _types.types.logicalOR || op === _types.types.logicalAND ? "LogicalExpression" : "BinaryExpression");
      return this.parseExprOp(node, leftStartPos, leftStartLoc, minPrec, noIn);
    }
  }
  return left;
};

// Parse unary operators, both prefix and postfix.

pp.parseMaybeUnary = function (refShorthandDefaultPos) {
  if (this.state.type.prefix) {
    var node = this.startNode();
    var update = this.match(_types.types.incDec);
    node.operator = this.state.value;
    node.prefix = true;
    this.next();

    var argType = this.state.type;
    this.addExtra(node, "parenthesizedArgument", argType === _types.types.parenL);
    node.argument = this.parseMaybeUnary();

    if (refShorthandDefaultPos && refShorthandDefaultPos.start) {
      this.unexpected(refShorthandDefaultPos.start);
    }

    if (update) {
      this.checkLVal(node.argument);
    } else if (this.state.strict && node.operator === "delete" && node.argument.type === "Identifier") {
      this.raise(node.start, "Deleting local variable in strict mode");
    }

    return this.finishNode(node, update ? "UpdateExpression" : "UnaryExpression");
  }

  var startPos = this.state.start,
      startLoc = this.state.startLoc;
  var expr = this.parseExprSubscripts(refShorthandDefaultPos);
  if (refShorthandDefaultPos && refShorthandDefaultPos.start) return expr;
  while (this.state.type.postfix && !this.canInsertSemicolon()) {
    var _node = this.startNodeAt(startPos, startLoc);
    _node.operator = this.state.value;
    _node.prefix = false;
    _node.argument = expr;
    this.checkLVal(expr);
    this.next();
    expr = this.finishNode(_node, "UpdateExpression");
  }
  return expr;
};

// Parse call, dot, and `[]`-subscript expressions.

pp.parseExprSubscripts = function (refShorthandDefaultPos) {
  var startPos = this.state.start,
      startLoc = this.state.startLoc;
  var potentialArrowAt = this.state.potentialArrowAt;
  var expr = this.parseExprAtom(refShorthandDefaultPos);

  if (expr.type === "ArrowFunctionExpression" && expr.start === potentialArrowAt) {
    return expr;
  }

  if (refShorthandDefaultPos && refShorthandDefaultPos.start) {
    return expr;
  }

  return this.parseSubscripts(expr, startPos, startLoc);
};

pp.parseSubscripts = function (base, startPos, startLoc, noCalls) {
  for (;;) {
    if (!noCalls && this.eat(_types.types.doubleColon)) {
      var node = this.startNodeAt(startPos, startLoc);
      node.object = base;
      node.callee = this.parseNoCallExpr();
      return this.parseSubscripts(this.finishNode(node, "BindExpression"), startPos, startLoc, noCalls);
    } else if (this.eat(_types.types.dot)) {
      var _node2 = this.startNodeAt(startPos, startLoc);
      _node2.object = base;
      _node2.property = this.parseIdentifier(true);
      _node2.computed = false;
      base = this.finishNode(_node2, "MemberExpression");
    } else if (this.eat(_types.types.bracketL)) {
      var _node3 = this.startNodeAt(startPos, startLoc);
      _node3.object = base;
      _node3.property = this.parseExpression();
      _node3.computed = true;
      this.expect(_types.types.bracketR);
      base = this.finishNode(_node3, "MemberExpression");
    } else if (!noCalls && this.match(_types.types.parenL)) {
      var possibleAsync = this.state.potentialArrowAt === base.start && base.type === "Identifier" && base.name === "async" && !this.canInsertSemicolon();
      this.next();

      var _node4 = this.startNodeAt(startPos, startLoc);
      _node4.callee = base;
      _node4.arguments = this.parseCallExpressionArguments(_types.types.parenR, this.hasPlugin("trailingFunctionCommas"), possibleAsync);
      base = this.finishNode(_node4, "CallExpression");

      if (possibleAsync && this.shouldParseAsyncArrow()) {
        return this.parseAsyncArrowFromCallExpression(this.startNodeAt(startPos, startLoc), _node4);
      } else {
        this.toReferencedList(_node4.arguments);
      }
    } else if (this.match(_types.types.backQuote)) {
      var _node5 = this.startNodeAt(startPos, startLoc);
      _node5.tag = base;
      _node5.quasi = this.parseTemplate();
      base = this.finishNode(_node5, "TaggedTemplateExpression");
    } else {
      return base;
    }
  }
};

pp.parseCallExpressionArguments = function (close, allowTrailingComma, possibleAsyncArrow) {
  var innerParenStart = void 0;

  var elts = [],
      first = true;
  while (!this.eat(close)) {
    if (first) {
      first = false;
    } else {
      this.expect(_types.types.comma);
      if (allowTrailingComma && this.eat(close)) break;
    }

    // we need to make sure that if this is an async arrow functions, that we don't allow inner parens inside the params
    if (this.match(_types.types.parenL) && !innerParenStart) {
      innerParenStart = this.state.start;
    }

    elts.push(this.parseExprListItem());
  }

  // we found an async arrow function so let's not allow any inner parens
  if (possibleAsyncArrow && innerParenStart && this.shouldParseAsyncArrow()) {
    this.unexpected();
  }

  return elts;
};

pp.shouldParseAsyncArrow = function () {
  return this.match(_types.types.arrow);
};

pp.parseAsyncArrowFromCallExpression = function (node, call) {
  if (!this.hasPlugin("asyncFunctions")) this.unexpected();
  this.expect(_types.types.arrow);
  return this.parseArrowExpression(node, call.arguments, true);
};

// Parse a no-call expression (like argument of `new` or `::` operators).

pp.parseNoCallExpr = function () {
  var startPos = this.state.start,
      startLoc = this.state.startLoc;
  return this.parseSubscripts(this.parseExprAtom(), startPos, startLoc, true);
};

// Parse an atomic expression — either a single token that is an
// expression, an expression started by a keyword like `function` or
// `new`, or an expression wrapped in punctuation like `()`, `[]`,
// or `{}`.

pp.parseExprAtom = function (refShorthandDefaultPos) {
  var node = void 0,
      canBeArrow = this.state.potentialArrowAt === this.state.start;
  switch (this.state.type) {
    case _types.types._super:
      if (!this.state.inMethod && !this.options.allowSuperOutsideMethod) {
        this.raise(this.state.start, "'super' outside of function or class");
      }

      node = this.startNode();
      this.next();
      if (!this.match(_types.types.parenL) && !this.match(_types.types.bracketL) && !this.match(_types.types.dot)) {
        this.unexpected();
      }
      if (this.match(_types.types.parenL) && this.state.inMethod !== "constructor" && !this.options.allowSuperOutsideMethod) {
        this.raise(node.start, "super() outside of class constructor");
      }
      return this.finishNode(node, "Super");

    case _types.types._this:
      node = this.startNode();
      this.next();
      return this.finishNode(node, "ThisExpression");

    case _types.types._yield:
      if (this.state.inGenerator) this.unexpected();

    case _types.types.name:
      node = this.startNode();
      var allowAwait = this.hasPlugin("asyncFunctions") && this.state.value === "await" && this.state.inAsync;
      var allowYield = this.shouldAllowYieldIdentifier();
      var id = this.parseIdentifier(allowAwait || allowYield);

      if (this.hasPlugin("asyncFunctions")) {
        if (id.name === "await") {
          if (this.state.inAsync || this.inModule) {
            return this.parseAwait(node);
          }
        } else if (id.name === "async" && this.match(_types.types._function) && !this.canInsertSemicolon()) {
          this.next();
          return this.parseFunction(node, false, false, true);
        } else if (canBeArrow && id.name === "async" && this.match(_types.types.name)) {
          var params = [this.parseIdentifier()];
          this.expect(_types.types.arrow);
          // let foo = bar => {};
          return this.parseArrowExpression(node, params, true);
        }
      }

      if (canBeArrow && !this.canInsertSemicolon() && this.eat(_types.types.arrow)) {
        return this.parseArrowExpression(node, [id]);
      }

      return id;

    case _types.types._do:
      if (this.hasPlugin("doExpressions")) {
        var _node6 = this.startNode();
        this.next();
        var oldInFunction = this.state.inFunction;
        var oldLabels = this.state.labels;
        this.state.labels = [];
        this.state.inFunction = false;
        _node6.body = this.parseBlock(false, true);
        this.state.inFunction = oldInFunction;
        this.state.labels = oldLabels;
        return this.finishNode(_node6, "DoExpression");
      }

    case _types.types.regexp:
      var value = this.state.value;
      node = this.parseLiteral(value.value, "RegExpLiteral");
      node.pattern = value.pattern;
      node.flags = value.flags;
      return node;

    case _types.types.num:
      return this.parseLiteral(this.state.value, "NumericLiteral");

    case _types.types.string:
      return this.parseLiteral(this.state.value, "StringLiteral");

    case _types.types._null:
      node = this.startNode();
      this.next();
      return this.finishNode(node, "NullLiteral");

    case _types.types._true:case _types.types._false:
      node = this.startNode();
      node.value = this.match(_types.types._true);
      this.next();
      return this.finishNode(node, "BooleanLiteral");

    case _types.types.parenL:
      return this.parseParenAndDistinguishExpression(null, null, canBeArrow);

    case _types.types.bracketL:
      node = this.startNode();
      this.next();
      node.elements = this.parseExprList(_types.types.bracketR, true, true, refShorthandDefaultPos);
      this.toReferencedList(node.elements);
      return this.finishNode(node, "ArrayExpression");

    case _types.types.braceL:
      return this.parseObj(false, refShorthandDefaultPos);

    case _types.types._function:
      return this.parseFunctionExpression();

    case _types.types.at:
      this.parseDecorators();

    case _types.types._class:
      node = this.startNode();
      this.takeDecorators(node);
      return this.parseClass(node, false);

    case _types.types._new:
      return this.parseNew();

    case _types.types.backQuote:
      return this.parseTemplate();

    case _types.types.doubleColon:
      node = this.startNode();
      this.next();
      node.object = null;
      var callee = node.callee = this.parseNoCallExpr();
      if (callee.type === "MemberExpression") {
        return this.finishNode(node, "BindExpression");
      } else {
        this.raise(callee.start, "Binding should be performed on object property.");
      }

    default:
      this.unexpected();
  }
};

pp.parseFunctionExpression = function () {
  var node = this.startNode();
  var meta = this.parseIdentifier(true);
  if (this.state.inGenerator && this.eat(_types.types.dot) && this.hasPlugin("functionSent")) {
    return this.parseMetaProperty(node, meta, "sent");
  } else {
    return this.parseFunction(node, false);
  }
};

pp.parseMetaProperty = function (node, meta, propertyName) {
  node.meta = meta;
  node.property = this.parseIdentifier(true);

  if (node.property.name !== propertyName) {
    this.raise(node.property.start, "The only valid meta property for new is " + meta.name + "." + propertyName);
  }

  return this.finishNode(node, "MetaProperty");
};

pp.parseLiteral = function (value, type) {
  var node = this.startNode();
  this.addExtra(node, "rawValue", value);
  this.addExtra(node, "raw", this.input.slice(this.state.start, this.state.end));
  node.value = value;
  this.next();
  return this.finishNode(node, type);
};

pp.parseParenExpression = function () {
  this.expect(_types.types.parenL);
  var val = this.parseExpression();
  this.expect(_types.types.parenR);
  return val;
};

pp.parseParenAndDistinguishExpression = function (startPos, startLoc, canBeArrow, isAsync) {
  startPos = startPos || this.state.start;
  startLoc = startLoc || this.state.startLoc;

  var val = void 0;
  this.expect(_types.types.parenL);

  var innerStartPos = this.state.start,
      innerStartLoc = this.state.startLoc;
  var exprList = [],
      first = true;
  var refShorthandDefaultPos = { start: 0 },
      spreadStart = void 0,
      optionalCommaStart = void 0;
  var refNeedsArrowPos = { start: 0 };
  while (!this.match(_types.types.parenR)) {
    if (first) {
      first = false;
    } else {
      this.expect(_types.types.comma);
      if (this.match(_types.types.parenR) && this.hasPlugin("trailingFunctionCommas")) {
        optionalCommaStart = this.state.start;
        break;
      }
    }

    if (this.match(_types.types.ellipsis)) {
      var spreadNodeStartPos = this.state.start,
          spreadNodeStartLoc = this.state.startLoc;
      spreadStart = this.state.start;
      exprList.push(this.parseParenItem(this.parseRest(), spreadNodeStartLoc, spreadNodeStartPos));
      break;
    } else {
      exprList.push(this.parseMaybeAssign(false, refShorthandDefaultPos, this.parseParenItem, refNeedsArrowPos));
    }
  }

  var innerEndPos = this.state.start;
  var innerEndLoc = this.state.startLoc;
  this.expect(_types.types.parenR);

  var arrowNode = this.startNodeAt(startPos, startLoc);
  if (canBeArrow && !this.canInsertSemicolon() && (arrowNode = this.parseArrow(arrowNode))) {
    var _iteratorNormalCompletion = true;
    var _didIteratorError = false;
    var _iteratorError = undefined;

    try {
      for (var _iterator = (0, _getIterator3.default)(exprList), _step; !(_iteratorNormalCompletion = (_step = _iterator.next()).done); _iteratorNormalCompletion = true) {
        var param = _step.value;

        if (param.extra && param.extra.parenthesized) this.unexpected(param.extra.parenStart);
      }
    } catch (err) {
      _didIteratorError = true;
      _iteratorError = err;
    } finally {
      try {
        if (!_iteratorNormalCompletion && _iterator.return) {
          _iterator.return();
        }
      } finally {
        if (_didIteratorError) {
          throw _iteratorError;
        }
      }
    }

    return this.parseArrowExpression(arrowNode, exprList, isAsync);
  }

  if (!exprList.length) {
    if (isAsync) {
      return;
    } else {
      this.unexpected(this.state.lastTokStart);
    }
  }
  if (optionalCommaStart) this.unexpected(optionalCommaStart);
  if (spreadStart) this.unexpected(spreadStart);
  if (refShorthandDefaultPos.start) this.unexpected(refShorthandDefaultPos.start);
  if (refNeedsArrowPos.start) this.unexpected(refNeedsArrowPos.start);

  if (exprList.length > 1) {
    val = this.startNodeAt(innerStartPos, innerStartLoc);
    val.expressions = exprList;
    this.toReferencedList(val.expressions);
    this.finishNodeAt(val, "SequenceExpression", innerEndPos, innerEndLoc);
  } else {
    val = exprList[0];
  }

  this.addExtra(val, "parenthesized", true);
  this.addExtra(val, "parenStart", startPos);

  return val;
};

pp.parseArrow = function (node) {
  if (this.eat(_types.types.arrow)) {
    return node;
  }
};

pp.parseParenItem = function (node) {
  return node;
};

// New's precedence is slightly tricky. It must allow its argument
// to be a `[]` or dot subscript expression, but not a call — at
// least, not without wrapping it in parentheses. Thus, it uses the

pp.parseNew = function () {
  var node = this.startNode();
  var meta = this.parseIdentifier(true);

  if (this.eat(_types.types.dot)) {
    return this.parseMetaProperty(node, meta, "target");
  }

  node.callee = this.parseNoCallExpr();

  if (this.eat(_types.types.parenL)) {
    node.arguments = this.parseExprList(_types.types.parenR, this.hasPlugin("trailingFunctionCommas"));
    this.toReferencedList(node.arguments);
  } else {
    node.arguments = [];
  }

  return this.finishNode(node, "NewExpression");
};

// Parse template expression.

pp.parseTemplateElement = function () {
  var elem = this.startNode();
  elem.value = {
    raw: this.input.slice(this.state.start, this.state.end).replace(/\r\n?/g, "\n"),
    cooked: this.state.value
  };
  this.next();
  elem.tail = this.match(_types.types.backQuote);
  return this.finishNode(elem, "TemplateElement");
};

pp.parseTemplate = function () {
  var node = this.startNode();
  this.next();
  node.expressions = [];
  var curElt = this.parseTemplateElement();
  node.quasis = [curElt];
  while (!curElt.tail) {
    this.expect(_types.types.dollarBraceL);
    node.expressions.push(this.parseExpression());
    this.expect(_types.types.braceR);
    node.quasis.push(curElt = this.parseTemplateElement());
  }
  this.next();
  return this.finishNode(node, "TemplateLiteral");
};

// Parse an object literal or binding pattern.

pp.parseObj = function (isPattern, refShorthandDefaultPos) {
  var decorators = [];
  var propHash = (0, _create2.default)(null);
  var first = true;
  var node = this.startNode();

  node.properties = [];
  this.next();

  while (!this.eat(_types.types.braceR)) {
    if (first) {
      first = false;
    } else {
      this.expect(_types.types.comma);
      if (this.eat(_types.types.braceR)) break;
    }

    while (this.match(_types.types.at)) {
      decorators.push(this.parseDecorator());
    }

    var prop = this.startNode(),
        isGenerator = false,
        isAsync = false,
        startPos = void 0,
        startLoc = void 0;
    if (decorators.length) {
      prop.decorators = decorators;
      decorators = [];
    }

    if (this.hasPlugin("objectRestSpread") && this.match(_types.types.ellipsis)) {
      prop = this.parseSpread();
      prop.type = isPattern ? "RestProperty" : "SpreadProperty";
      node.properties.push(prop);
      continue;
    }

    prop.method = false;
    prop.shorthand = false;

    if (isPattern || refShorthandDefaultPos) {
      startPos = this.state.start;
      startLoc = this.state.startLoc;
    }

    if (!isPattern) {
      isGenerator = this.eat(_types.types.star);
    }

    if (!isPattern && this.hasPlugin("asyncFunctions") && this.isContextual("async")) {
      if (isGenerator) this.unexpected();

      var asyncId = this.parseIdentifier();
      if (this.match(_types.types.colon) || this.match(_types.types.parenL) || this.match(_types.types.braceR)) {
        prop.key = asyncId;
      } else {
        isAsync = true;
        if (this.hasPlugin("asyncGenerators")) isGenerator = this.eat(_types.types.star);
        this.parsePropertyName(prop);
      }
    } else {
      this.parsePropertyName(prop);
    }

    this.parseObjPropValue(prop, startPos, startLoc, isGenerator, isAsync, isPattern, refShorthandDefaultPos);
    this.checkPropClash(prop, propHash);

    if (prop.shorthand) {
      this.addExtra(prop, "shorthand", true);
    }

    node.properties.push(prop);
  }

  if (decorators.length) {
    this.raise(this.state.start, "You have trailing decorators with no property");
  }

  return this.finishNode(node, isPattern ? "ObjectPattern" : "ObjectExpression");
};

pp.parseObjPropValue = function (prop, startPos, startLoc, isGenerator, isAsync, isPattern, refShorthandDefaultPos) {
  if (isAsync || isGenerator || this.match(_types.types.parenL)) {
    if (isPattern) this.unexpected();
    prop.kind = "method";
    prop.method = true;
    this.parseMethod(prop, isGenerator, isAsync);
    return this.finishNode(prop, "ObjectMethod");
  }

  if (this.eat(_types.types.colon)) {
    prop.value = isPattern ? this.parseMaybeDefault(this.state.start, this.state.startLoc) : this.parseMaybeAssign(false, refShorthandDefaultPos);
    return this.finishNode(prop, "ObjectProperty");
  }

  if (!prop.computed && prop.key.type === "Identifier" && (prop.key.name === "get" || prop.key.name === "set") && !this.match(_types.types.comma) && !this.match(_types.types.braceR)) {
    if (isGenerator || isAsync || isPattern) this.unexpected();
    prop.kind = prop.key.name;
    this.parsePropertyName(prop);
    this.parseMethod(prop, false);
    var paramCount = prop.kind === "get" ? 0 : 1;
    if (prop.params.length !== paramCount) {
      var start = prop.start;
      if (prop.kind === "get") {
        this.raise(start, "getter should have no params");
      } else {
        this.raise(start, "setter should have exactly one param");
      }
    }
    return this.finishNode(prop, "ObjectMethod");
  }

  if (!prop.computed && prop.key.type === "Identifier") {
    if (isPattern) {
      var illegalBinding = this.isKeyword(prop.key.name);
      if (!illegalBinding && this.state.strict) {
        illegalBinding = _identifier.reservedWords.strictBind(prop.key.name) || _identifier.reservedWords.strict(prop.key.name);
      }
      if (illegalBinding) {
        this.raise(prop.key.start, "Binding " + prop.key.name);
      }
      prop.value = this.parseMaybeDefault(startPos, startLoc, prop.key.__clone());
    } else if (this.match(_types.types.eq) && refShorthandDefaultPos) {
      if (!refShorthandDefaultPos.start) {
        refShorthandDefaultPos.start = this.state.start;
      }
      prop.value = this.parseMaybeDefault(startPos, startLoc, prop.key.__clone());
    } else {
      prop.value = prop.key.__clone();
    }
    prop.shorthand = true;
    return this.finishNode(prop, "ObjectProperty");
  }

  this.unexpected();
};

pp.parsePropertyName = function (prop) {
  if (this.eat(_types.types.bracketL)) {
    prop.computed = true;
    prop.key = this.parseMaybeAssign();
    this.expect(_types.types.bracketR);
    return prop.key;
  } else {
    prop.computed = false;
    return prop.key = this.match(_types.types.num) || this.match(_types.types.string) ? this.parseExprAtom() : this.parseIdentifier(true);
  }
};

// Initialize empty function node.

pp.initFunction = function (node, isAsync) {
  node.id = null;
  node.generator = false;
  node.expression = false;
  if (this.hasPlugin("asyncFunctions")) {
    node.async = !!isAsync;
  }
};

// Parse object or class method.

pp.parseMethod = function (node, isGenerator, isAsync) {
  var oldInMethod = this.state.inMethod;
  this.state.inMethod = node.kind || true;
  this.initFunction(node, isAsync);
  this.expect(_types.types.parenL);
  node.params = this.parseBindingList(_types.types.parenR, false, this.hasPlugin("trailingFunctionCommas"));
  node.generator = isGenerator;
  this.parseFunctionBody(node);
  this.state.inMethod = oldInMethod;
  return node;
};

// Parse arrow function expression with given parameters.

pp.parseArrowExpression = function (node, params, isAsync) {
  this.initFunction(node, isAsync);
  node.params = this.toAssignableList(params, true);
  this.parseFunctionBody(node, true);
  return this.finishNode(node, "ArrowFunctionExpression");
};

// Parse function body and check parameters.

pp.parseFunctionBody = function (node, allowExpression) {
  var isExpression = allowExpression && !this.match(_types.types.braceL);

  var oldInAsync = this.state.inAsync;
  this.state.inAsync = node.async;
  if (isExpression) {
    node.body = this.parseMaybeAssign();
    node.expression = true;
  } else {
    // Start a new scope with regard to labels and the `inFunction`
    // flag (restore them to their old value afterwards).
    var oldInFunc = this.state.inFunction,
        oldInGen = this.state.inGenerator,
        oldLabels = this.state.labels;
    this.state.inFunction = true;this.state.inGenerator = node.generator;this.state.labels = [];
    node.body = this.parseBlock(true);
    node.expression = false;
    this.state.inFunction = oldInFunc;this.state.inGenerator = oldInGen;this.state.labels = oldLabels;
  }
  this.state.inAsync = oldInAsync;

  // If this is a strict mode function, verify that argument names
  // are not repeated, and it does not try to bind the words `eval`
  // or `arguments`.
  var checkLVal = this.state.strict;
  var checkLValStrict = false;
  var isStrict = false;

  // arrow function
  if (allowExpression) checkLVal = true;

  // normal function
  if (!isExpression && node.body.directives.length) {
    var _iteratorNormalCompletion2 = true;
    var _didIteratorError2 = false;
    var _iteratorError2 = undefined;

    try {
      for (var _iterator2 = (0, _getIterator3.default)(node.body.directives), _step2; !(_iteratorNormalCompletion2 = (_step2 = _iterator2.next()).done); _iteratorNormalCompletion2 = true) {
        var directive = _step2.value;

        if (directive.value.value === "use strict") {
          isStrict = true;
          checkLVal = true;
          checkLValStrict = true;
          break;
        }
      }
    } catch (err) {
      _didIteratorError2 = true;
      _iteratorError2 = err;
    } finally {
      try {
        if (!_iteratorNormalCompletion2 && _iterator2.return) {
          _iterator2.return();
        }
      } finally {
        if (_didIteratorError2) {
          throw _iteratorError2;
        }
      }
    }
  }

  //
  if (isStrict && node.id && node.id.type === "Identifier" && node.id.name === "yield") {
    this.raise(node.id.start, "Binding yield in strict mode");
  }

  if (checkLVal) {
    var nameHash = (0, _create2.default)(null);
    var oldStrict = this.state.strict;
    if (checkLValStrict) this.state.strict = true;
    if (node.id) {
      this.checkLVal(node.id, true);
    }
    var _iteratorNormalCompletion3 = true;
    var _didIteratorError3 = false;
    var _iteratorError3 = undefined;

    try {
      for (var _iterator3 = (0, _getIterator3.default)(node.params), _step3; !(_iteratorNormalCompletion3 = (_step3 = _iterator3.next()).done); _iteratorNormalCompletion3 = true) {
        var param = _step3.value;

        this.checkLVal(param, true, nameHash);
      }
    } catch (err) {
      _didIteratorError3 = true;
      _iteratorError3 = err;
    } finally {
      try {
        if (!_iteratorNormalCompletion3 && _iterator3.return) {
          _iterator3.return();
        }
      } finally {
        if (_didIteratorError3) {
          throw _iteratorError3;
        }
      }
    }

    this.state.strict = oldStrict;
  }
};

// Parses a comma-separated list of expressions, and returns them as
// an array. `close` is the token type that ends the list, and
// `allowEmpty` can be turned on to allow subsequent commas with
// nothing in between them to be parsed as `null` (which is needed
// for array literals).

pp.parseExprList = function (close, allowTrailingComma, allowEmpty, refShorthandDefaultPos) {
  var elts = [],
      first = true;
  while (!this.eat(close)) {
    if (first) {
      first = false;
    } else {
      this.expect(_types.types.comma);
      if (allowTrailingComma && this.eat(close)) break;
    }

    elts.push(this.parseExprListItem(allowEmpty, refShorthandDefaultPos));
  }
  return elts;
};

pp.parseExprListItem = function (allowEmpty, refShorthandDefaultPos) {
  var elt = void 0;
  if (allowEmpty && this.match(_types.types.comma)) {
    elt = null;
  } else if (this.match(_types.types.ellipsis)) {
    elt = this.parseSpread(refShorthandDefaultPos);
  } else {
    elt = this.parseMaybeAssign(false, refShorthandDefaultPos);
  }
  return elt;
};

// Parse the next token as an identifier. If `liberal` is true (used
// when parsing properties), it will also convert keywords into
// identifiers.

pp.parseIdentifier = function (liberal) {
  var node = this.startNode();

  if (this.match(_types.types.name)) {
    if (!liberal && this.state.strict && _identifier.reservedWords.strict(this.state.value)) {
      this.raise(this.state.start, "The keyword '" + this.state.value + "' is reserved");
    }

    node.name = this.state.value;
  } else if (liberal && this.state.type.keyword) {
    node.name = this.state.type.keyword;
  } else {
    this.unexpected();
  }

  if (!liberal && node.name === "await" && this.state.inAsync) {
    this.raise(node.start, "invalid use of await inside of an async function");
  }

  this.next();
  return this.finishNode(node, "Identifier");
};

// Parses await expression inside async function.

pp.parseAwait = function (node) {
  if (!this.state.inAsync) {
    this.unexpected();
  }
  if (this.match(_types.types.star)) {
    this.raise(node.start, "await* has been removed from the async functions proposal. Use Promise.all() instead.");
  }
  node.argument = this.parseMaybeUnary();
  return this.finishNode(node, "AwaitExpression");
};

// Parses yield expression inside generator.

pp.parseYield = function () {
  var node = this.startNode();
  this.next();
  if (this.match(_types.types.semi) || this.canInsertSemicolon() || !this.match(_types.types.star) && !this.state.type.startsExpr) {
    node.delegate = false;
    node.argument = null;
  } else {
    node.delegate = this.eat(_types.types.star);
    node.argument = this.parseMaybeAssign();
  }
  return this.finishNode(node, "YieldExpression");
};