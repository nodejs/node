"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;
var _types = require("../tokenizer/types");
var _lval = require("./lval");
var _identifier = require("../util/identifier");
var _location = require("../util/location");
var _scopeflags = require("../util/scopeflags");
var _util = require("./util");
var _productionParameter = require("../util/production-parameter");
var _expressionScope = require("../util/expression-scope");
var _parseError = require("../parse-error");
var _pipelineOperatorErrors = require("../parse-error/pipeline-operator-errors");
var _comments = require("./comments");
var _node = require("./node");
class ExpressionParser extends _lval.default {
  checkProto(prop, isRecord, protoRef, refExpressionErrors) {
    if (prop.type === "SpreadElement" || this.isObjectMethod(prop) || prop.computed || prop.shorthand) {
      return;
    }
    const key = prop.key;
    const name = key.type === "Identifier" ? key.name : key.value;
    if (name === "__proto__") {
      if (isRecord) {
        this.raise(_parseError.Errors.RecordNoProto, {
          at: key
        });
        return;
      }
      if (protoRef.used) {
        if (refExpressionErrors) {
          if (refExpressionErrors.doubleProtoLoc === null) {
            refExpressionErrors.doubleProtoLoc = key.loc.start;
          }
        } else {
          this.raise(_parseError.Errors.DuplicateProto, {
            at: key
          });
        }
      }
      protoRef.used = true;
    }
  }
  shouldExitDescending(expr, potentialArrowAt) {
    return expr.type === "ArrowFunctionExpression" && expr.start === potentialArrowAt;
  }
  getExpression() {
    this.enterInitialScopes();
    this.nextToken();
    const expr = this.parseExpression();
    if (!this.match(137)) {
      this.unexpected();
    }
    this.finalizeRemainingComments();
    expr.comments = this.state.comments;
    expr.errors = this.state.errors;
    if (this.options.tokens) {
      expr.tokens = this.tokens;
    }
    return expr;
  }
  parseExpression(disallowIn, refExpressionErrors) {
    if (disallowIn) {
      return this.disallowInAnd(() => this.parseExpressionBase(refExpressionErrors));
    }
    return this.allowInAnd(() => this.parseExpressionBase(refExpressionErrors));
  }
  parseExpressionBase(refExpressionErrors) {
    const startLoc = this.state.startLoc;
    const expr = this.parseMaybeAssign(refExpressionErrors);
    if (this.match(12)) {
      const node = this.startNodeAt(startLoc);
      node.expressions = [expr];
      while (this.eat(12)) {
        node.expressions.push(this.parseMaybeAssign(refExpressionErrors));
      }
      this.toReferencedList(node.expressions);
      return this.finishNode(node, "SequenceExpression");
    }
    return expr;
  }
  parseMaybeAssignDisallowIn(refExpressionErrors, afterLeftParse) {
    return this.disallowInAnd(() => this.parseMaybeAssign(refExpressionErrors, afterLeftParse));
  }
  parseMaybeAssignAllowIn(refExpressionErrors, afterLeftParse) {
    return this.allowInAnd(() => this.parseMaybeAssign(refExpressionErrors, afterLeftParse));
  }
  setOptionalParametersError(refExpressionErrors, resultError) {
    var _resultError$loc;
    refExpressionErrors.optionalParametersLoc = (_resultError$loc = resultError == null ? void 0 : resultError.loc) != null ? _resultError$loc : this.state.startLoc;
  }
  parseMaybeAssign(refExpressionErrors, afterLeftParse) {
    const startLoc = this.state.startLoc;
    if (this.isContextual(106)) {
      if (this.prodParam.hasYield) {
        let left = this.parseYield();
        if (afterLeftParse) {
          left = afterLeftParse.call(this, left, startLoc);
        }
        return left;
      }
    }
    let ownExpressionErrors;
    if (refExpressionErrors) {
      ownExpressionErrors = false;
    } else {
      refExpressionErrors = new _util.ExpressionErrors();
      ownExpressionErrors = true;
    }
    const {
      type
    } = this.state;
    if (type === 10 || (0, _types.tokenIsIdentifier)(type)) {
      this.state.potentialArrowAt = this.state.start;
    }
    let left = this.parseMaybeConditional(refExpressionErrors);
    if (afterLeftParse) {
      left = afterLeftParse.call(this, left, startLoc);
    }
    if ((0, _types.tokenIsAssignment)(this.state.type)) {
      const node = this.startNodeAt(startLoc);
      const operator = this.state.value;
      node.operator = operator;
      if (this.match(29)) {
        this.toAssignable(left, true);
        node.left = left;
        const startIndex = startLoc.index;
        if (refExpressionErrors.doubleProtoLoc != null && refExpressionErrors.doubleProtoLoc.index >= startIndex) {
          refExpressionErrors.doubleProtoLoc = null;
        }
        if (refExpressionErrors.shorthandAssignLoc != null && refExpressionErrors.shorthandAssignLoc.index >= startIndex) {
          refExpressionErrors.shorthandAssignLoc = null;
        }
        if (refExpressionErrors.privateKeyLoc != null && refExpressionErrors.privateKeyLoc.index >= startIndex) {
          this.checkDestructuringPrivate(refExpressionErrors);
          refExpressionErrors.privateKeyLoc = null;
        }
      } else {
        node.left = left;
      }
      this.next();
      node.right = this.parseMaybeAssign();
      this.checkLVal(left, {
        in: this.finishNode(node, "AssignmentExpression")
      });
      return node;
    } else if (ownExpressionErrors) {
      this.checkExpressionErrors(refExpressionErrors, true);
    }
    return left;
  }
  parseMaybeConditional(refExpressionErrors) {
    const startLoc = this.state.startLoc;
    const potentialArrowAt = this.state.potentialArrowAt;
    const expr = this.parseExprOps(refExpressionErrors);
    if (this.shouldExitDescending(expr, potentialArrowAt)) {
      return expr;
    }
    return this.parseConditional(expr, startLoc, refExpressionErrors);
  }
  parseConditional(expr, startLoc, refExpressionErrors) {
    if (this.eat(17)) {
      const node = this.startNodeAt(startLoc);
      node.test = expr;
      node.consequent = this.parseMaybeAssignAllowIn();
      this.expect(14);
      node.alternate = this.parseMaybeAssign();
      return this.finishNode(node, "ConditionalExpression");
    }
    return expr;
  }
  parseMaybeUnaryOrPrivate(refExpressionErrors) {
    return this.match(136) ? this.parsePrivateName() : this.parseMaybeUnary(refExpressionErrors);
  }
  parseExprOps(refExpressionErrors) {
    const startLoc = this.state.startLoc;
    const potentialArrowAt = this.state.potentialArrowAt;
    const expr = this.parseMaybeUnaryOrPrivate(refExpressionErrors);
    if (this.shouldExitDescending(expr, potentialArrowAt)) {
      return expr;
    }
    return this.parseExprOp(expr, startLoc, -1);
  }
  parseExprOp(left, leftStartLoc, minPrec) {
    if (this.isPrivateName(left)) {
      const value = this.getPrivateNameSV(left);
      if (minPrec >= (0, _types.tokenOperatorPrecedence)(58) || !this.prodParam.hasIn || !this.match(58)) {
        this.raise(_parseError.Errors.PrivateInExpectedIn, {
          at: left,
          identifierName: value
        });
      }
      this.classScope.usePrivateName(value, left.loc.start);
    }
    const op = this.state.type;
    if ((0, _types.tokenIsOperator)(op) && (this.prodParam.hasIn || !this.match(58))) {
      let prec = (0, _types.tokenOperatorPrecedence)(op);
      if (prec > minPrec) {
        if (op === 39) {
          this.expectPlugin("pipelineOperator");
          if (this.state.inFSharpPipelineDirectBody) {
            return left;
          }
          this.checkPipelineAtInfixOperator(left, leftStartLoc);
        }
        const node = this.startNodeAt(leftStartLoc);
        node.left = left;
        node.operator = this.state.value;
        const logical = op === 41 || op === 42;
        const coalesce = op === 40;
        if (coalesce) {
          prec = (0, _types.tokenOperatorPrecedence)(42);
        }
        this.next();
        if (op === 39 && this.hasPlugin(["pipelineOperator", {
          proposal: "minimal"
        }])) {
          if (this.state.type === 96 && this.prodParam.hasAwait) {
            throw this.raise(_parseError.Errors.UnexpectedAwaitAfterPipelineBody, {
              at: this.state.startLoc
            });
          }
        }
        node.right = this.parseExprOpRightExpr(op, prec);
        const finishedNode = this.finishNode(node, logical || coalesce ? "LogicalExpression" : "BinaryExpression");
        const nextOp = this.state.type;
        if (coalesce && (nextOp === 41 || nextOp === 42) || logical && nextOp === 40) {
          throw this.raise(_parseError.Errors.MixingCoalesceWithLogical, {
            at: this.state.startLoc
          });
        }
        return this.parseExprOp(finishedNode, leftStartLoc, minPrec);
      }
    }
    return left;
  }
  parseExprOpRightExpr(op, prec) {
    const startLoc = this.state.startLoc;
    switch (op) {
      case 39:
        switch (this.getPluginOption("pipelineOperator", "proposal")) {
          case "hack":
            return this.withTopicBindingContext(() => {
              return this.parseHackPipeBody();
            });
          case "smart":
            return this.withTopicBindingContext(() => {
              if (this.prodParam.hasYield && this.isContextual(106)) {
                throw this.raise(_parseError.Errors.PipeBodyIsTighter, {
                  at: this.state.startLoc
                });
              }
              return this.parseSmartPipelineBodyInStyle(this.parseExprOpBaseRightExpr(op, prec), startLoc);
            });
          case "fsharp":
            return this.withSoloAwaitPermittingContext(() => {
              return this.parseFSharpPipelineBody(prec);
            });
        }
      default:
        return this.parseExprOpBaseRightExpr(op, prec);
    }
  }
  parseExprOpBaseRightExpr(op, prec) {
    const startLoc = this.state.startLoc;
    return this.parseExprOp(this.parseMaybeUnaryOrPrivate(), startLoc, (0, _types.tokenIsRightAssociative)(op) ? prec - 1 : prec);
  }
  parseHackPipeBody() {
    var _body$extra;
    const {
      startLoc
    } = this.state;
    const body = this.parseMaybeAssign();
    const requiredParentheses = _pipelineOperatorErrors.UnparenthesizedPipeBodyDescriptions.has(body.type);
    if (requiredParentheses && !((_body$extra = body.extra) != null && _body$extra.parenthesized)) {
      this.raise(_parseError.Errors.PipeUnparenthesizedBody, {
        at: startLoc,
        type: body.type
      });
    }
    if (!this.topicReferenceWasUsedInCurrentContext()) {
      this.raise(_parseError.Errors.PipeTopicUnused, {
        at: startLoc
      });
    }
    return body;
  }
  checkExponentialAfterUnary(node) {
    if (this.match(57)) {
      this.raise(_parseError.Errors.UnexpectedTokenUnaryExponentiation, {
        at: node.argument
      });
    }
  }
  parseMaybeUnary(refExpressionErrors, sawUnary) {
    const startLoc = this.state.startLoc;
    const isAwait = this.isContextual(96);
    if (isAwait && this.isAwaitAllowed()) {
      this.next();
      const expr = this.parseAwait(startLoc);
      if (!sawUnary) this.checkExponentialAfterUnary(expr);
      return expr;
    }
    const update = this.match(34);
    const node = this.startNode();
    if ((0, _types.tokenIsPrefix)(this.state.type)) {
      node.operator = this.state.value;
      node.prefix = true;
      if (this.match(72)) {
        this.expectPlugin("throwExpressions");
      }
      const isDelete = this.match(89);
      this.next();
      node.argument = this.parseMaybeUnary(null, true);
      this.checkExpressionErrors(refExpressionErrors, true);
      if (this.state.strict && isDelete) {
        const arg = node.argument;
        if (arg.type === "Identifier") {
          this.raise(_parseError.Errors.StrictDelete, {
            at: node
          });
        } else if (this.hasPropertyAsPrivateName(arg)) {
          this.raise(_parseError.Errors.DeletePrivateField, {
            at: node
          });
        }
      }
      if (!update) {
        if (!sawUnary) {
          this.checkExponentialAfterUnary(node);
        }
        return this.finishNode(node, "UnaryExpression");
      }
    }
    const expr = this.parseUpdate(node, update, refExpressionErrors);
    if (isAwait) {
      const {
        type
      } = this.state;
      const startsExpr = this.hasPlugin("v8intrinsic") ? (0, _types.tokenCanStartExpression)(type) : (0, _types.tokenCanStartExpression)(type) && !this.match(54);
      if (startsExpr && !this.isAmbiguousAwait()) {
        this.raiseOverwrite(_parseError.Errors.AwaitNotInAsyncContext, {
          at: startLoc
        });
        return this.parseAwait(startLoc);
      }
    }
    return expr;
  }
  parseUpdate(node, update, refExpressionErrors) {
    if (update) {
      const updateExpressionNode = node;
      this.checkLVal(updateExpressionNode.argument, {
        in: this.finishNode(updateExpressionNode, "UpdateExpression")
      });
      return node;
    }
    const startLoc = this.state.startLoc;
    let expr = this.parseExprSubscripts(refExpressionErrors);
    if (this.checkExpressionErrors(refExpressionErrors, false)) return expr;
    while ((0, _types.tokenIsPostfix)(this.state.type) && !this.canInsertSemicolon()) {
      const node = this.startNodeAt(startLoc);
      node.operator = this.state.value;
      node.prefix = false;
      node.argument = expr;
      this.next();
      this.checkLVal(expr, {
        in: expr = this.finishNode(node, "UpdateExpression")
      });
    }
    return expr;
  }
  parseExprSubscripts(refExpressionErrors) {
    const startLoc = this.state.startLoc;
    const potentialArrowAt = this.state.potentialArrowAt;
    const expr = this.parseExprAtom(refExpressionErrors);
    if (this.shouldExitDescending(expr, potentialArrowAt)) {
      return expr;
    }
    return this.parseSubscripts(expr, startLoc);
  }
  parseSubscripts(base, startLoc, noCalls) {
    const state = {
      optionalChainMember: false,
      maybeAsyncArrow: this.atPossibleAsyncArrow(base),
      stop: false
    };
    do {
      base = this.parseSubscript(base, startLoc, noCalls, state);
      state.maybeAsyncArrow = false;
    } while (!state.stop);
    return base;
  }
  parseSubscript(base, startLoc, noCalls, state) {
    const {
      type
    } = this.state;
    if (!noCalls && type === 15) {
      return this.parseBind(base, startLoc, noCalls, state);
    } else if ((0, _types.tokenIsTemplate)(type)) {
      return this.parseTaggedTemplateExpression(base, startLoc, state);
    }
    let optional = false;
    if (type === 18) {
      if (noCalls) {
        this.raise(_parseError.Errors.OptionalChainingNoNew, {
          at: this.state.startLoc
        });
        if (this.lookaheadCharCode() === 40) {
          state.stop = true;
          return base;
        }
      }
      state.optionalChainMember = optional = true;
      this.next();
    }
    if (!noCalls && this.match(10)) {
      return this.parseCoverCallAndAsyncArrowHead(base, startLoc, state, optional);
    } else {
      const computed = this.eat(0);
      if (computed || optional || this.eat(16)) {
        return this.parseMember(base, startLoc, state, computed, optional);
      } else {
        state.stop = true;
        return base;
      }
    }
  }
  parseMember(base, startLoc, state, computed, optional) {
    const node = this.startNodeAt(startLoc);
    node.object = base;
    node.computed = computed;
    if (computed) {
      node.property = this.parseExpression();
      this.expect(3);
    } else if (this.match(136)) {
      if (base.type === "Super") {
        this.raise(_parseError.Errors.SuperPrivateField, {
          at: startLoc
        });
      }
      this.classScope.usePrivateName(this.state.value, this.state.startLoc);
      node.property = this.parsePrivateName();
    } else {
      node.property = this.parseIdentifier(true);
    }
    if (state.optionalChainMember) {
      node.optional = optional;
      return this.finishNode(node, "OptionalMemberExpression");
    } else {
      return this.finishNode(node, "MemberExpression");
    }
  }
  parseBind(base, startLoc, noCalls, state) {
    const node = this.startNodeAt(startLoc);
    node.object = base;
    this.next();
    node.callee = this.parseNoCallExpr();
    state.stop = true;
    return this.parseSubscripts(this.finishNode(node, "BindExpression"), startLoc, noCalls);
  }
  parseCoverCallAndAsyncArrowHead(base, startLoc, state, optional) {
    const oldMaybeInArrowParameters = this.state.maybeInArrowParameters;
    let refExpressionErrors = null;
    this.state.maybeInArrowParameters = true;
    this.next();
    const node = this.startNodeAt(startLoc);
    node.callee = base;
    const {
      maybeAsyncArrow,
      optionalChainMember
    } = state;
    if (maybeAsyncArrow) {
      this.expressionScope.enter((0, _expressionScope.newAsyncArrowScope)());
      refExpressionErrors = new _util.ExpressionErrors();
    }
    if (optionalChainMember) {
      node.optional = optional;
    }
    if (optional) {
      node.arguments = this.parseCallExpressionArguments(11);
    } else {
      node.arguments = this.parseCallExpressionArguments(11, base.type === "Import", base.type !== "Super", node, refExpressionErrors);
    }
    let finishedNode = this.finishCallExpression(node, optionalChainMember);
    if (maybeAsyncArrow && this.shouldParseAsyncArrow() && !optional) {
      state.stop = true;
      this.checkDestructuringPrivate(refExpressionErrors);
      this.expressionScope.validateAsPattern();
      this.expressionScope.exit();
      finishedNode = this.parseAsyncArrowFromCallExpression(this.startNodeAt(startLoc), finishedNode);
    } else {
      if (maybeAsyncArrow) {
        this.checkExpressionErrors(refExpressionErrors, true);
        this.expressionScope.exit();
      }
      this.toReferencedArguments(finishedNode);
    }
    this.state.maybeInArrowParameters = oldMaybeInArrowParameters;
    return finishedNode;
  }
  toReferencedArguments(node, isParenthesizedExpr) {
    this.toReferencedListDeep(node.arguments, isParenthesizedExpr);
  }
  parseTaggedTemplateExpression(base, startLoc, state) {
    const node = this.startNodeAt(startLoc);
    node.tag = base;
    node.quasi = this.parseTemplate(true);
    if (state.optionalChainMember) {
      this.raise(_parseError.Errors.OptionalChainingNoTemplate, {
        at: startLoc
      });
    }
    return this.finishNode(node, "TaggedTemplateExpression");
  }
  atPossibleAsyncArrow(base) {
    return base.type === "Identifier" && base.name === "async" && this.state.lastTokEndLoc.index === base.end && !this.canInsertSemicolon() && base.end - base.start === 5 && base.start === this.state.potentialArrowAt;
  }
  expectImportAttributesPlugin() {
    if (!this.hasPlugin("importAssertions")) {
      this.expectPlugin("importAttributes");
    }
  }
  finishCallExpression(node, optional) {
    if (node.callee.type === "Import") {
      if (node.arguments.length === 2) {
        {
          if (!this.hasPlugin("moduleAttributes")) {
            this.expectImportAttributesPlugin();
          }
        }
      }
      if (node.arguments.length === 0 || node.arguments.length > 2) {
        this.raise(_parseError.Errors.ImportCallArity, {
          at: node,
          maxArgumentCount: this.hasPlugin("importAttributes") || this.hasPlugin("importAssertions") || this.hasPlugin("moduleAttributes") ? 2 : 1
        });
      } else {
        for (const arg of node.arguments) {
          if (arg.type === "SpreadElement") {
            this.raise(_parseError.Errors.ImportCallSpreadArgument, {
              at: arg
            });
          }
        }
      }
    }
    return this.finishNode(node, optional ? "OptionalCallExpression" : "CallExpression");
  }
  parseCallExpressionArguments(close, dynamicImport, allowPlaceholder, nodeForExtra, refExpressionErrors) {
    const elts = [];
    let first = true;
    const oldInFSharpPipelineDirectBody = this.state.inFSharpPipelineDirectBody;
    this.state.inFSharpPipelineDirectBody = false;
    while (!this.eat(close)) {
      if (first) {
        first = false;
      } else {
        this.expect(12);
        if (this.match(close)) {
          if (dynamicImport && !this.hasPlugin("importAttributes") && !this.hasPlugin("importAssertions") && !this.hasPlugin("moduleAttributes")) {
            this.raise(_parseError.Errors.ImportCallArgumentTrailingComma, {
              at: this.state.lastTokStartLoc
            });
          }
          if (nodeForExtra) {
            this.addTrailingCommaExtraToNode(nodeForExtra);
          }
          this.next();
          break;
        }
      }
      elts.push(this.parseExprListItem(false, refExpressionErrors, allowPlaceholder));
    }
    this.state.inFSharpPipelineDirectBody = oldInFSharpPipelineDirectBody;
    return elts;
  }
  shouldParseAsyncArrow() {
    return this.match(19) && !this.canInsertSemicolon();
  }
  parseAsyncArrowFromCallExpression(node, call) {
    var _call$extra;
    this.resetPreviousNodeTrailingComments(call);
    this.expect(19);
    this.parseArrowExpression(node, call.arguments, true, (_call$extra = call.extra) == null ? void 0 : _call$extra.trailingCommaLoc);
    if (call.innerComments) {
      (0, _comments.setInnerComments)(node, call.innerComments);
    }
    if (call.callee.trailingComments) {
      (0, _comments.setInnerComments)(node, call.callee.trailingComments);
    }
    return node;
  }
  parseNoCallExpr() {
    const startLoc = this.state.startLoc;
    return this.parseSubscripts(this.parseExprAtom(), startLoc, true);
  }
  parseExprAtom(refExpressionErrors) {
    let node;
    let decorators = null;
    const {
      type
    } = this.state;
    switch (type) {
      case 79:
        return this.parseSuper();
      case 83:
        node = this.startNode();
        this.next();
        if (this.match(16)) {
          return this.parseImportMetaProperty(node);
        }
        if (!this.match(10)) {
          this.raise(_parseError.Errors.UnsupportedImport, {
            at: this.state.lastTokStartLoc
          });
        }
        return this.finishNode(node, "Import");
      case 78:
        node = this.startNode();
        this.next();
        return this.finishNode(node, "ThisExpression");
      case 90:
        {
          return this.parseDo(this.startNode(), false);
        }
      case 56:
      case 31:
        {
          this.readRegexp();
          return this.parseRegExpLiteral(this.state.value);
        }
      case 132:
        return this.parseNumericLiteral(this.state.value);
      case 133:
        return this.parseBigIntLiteral(this.state.value);
      case 134:
        return this.parseDecimalLiteral(this.state.value);
      case 131:
        return this.parseStringLiteral(this.state.value);
      case 84:
        return this.parseNullLiteral();
      case 85:
        return this.parseBooleanLiteral(true);
      case 86:
        return this.parseBooleanLiteral(false);
      case 10:
        {
          const canBeArrow = this.state.potentialArrowAt === this.state.start;
          return this.parseParenAndDistinguishExpression(canBeArrow);
        }
      case 2:
      case 1:
        {
          return this.parseArrayLike(this.state.type === 2 ? 4 : 3, false, true);
        }
      case 0:
        {
          return this.parseArrayLike(3, true, false, refExpressionErrors);
        }
      case 6:
      case 7:
        {
          return this.parseObjectLike(this.state.type === 6 ? 9 : 8, false, true);
        }
      case 5:
        {
          return this.parseObjectLike(8, false, false, refExpressionErrors);
        }
      case 68:
        return this.parseFunctionOrFunctionSent();
      case 26:
        decorators = this.parseDecorators();
      case 80:
        return this.parseClass(this.maybeTakeDecorators(decorators, this.startNode()), false);
      case 77:
        return this.parseNewOrNewTarget();
      case 25:
      case 24:
        return this.parseTemplate(false);
      case 15:
        {
          node = this.startNode();
          this.next();
          node.object = null;
          const callee = node.callee = this.parseNoCallExpr();
          if (callee.type === "MemberExpression") {
            return this.finishNode(node, "BindExpression");
          } else {
            throw this.raise(_parseError.Errors.UnsupportedBind, {
              at: callee
            });
          }
        }
      case 136:
        {
          this.raise(_parseError.Errors.PrivateInExpectedIn, {
            at: this.state.startLoc,
            identifierName: this.state.value
          });
          return this.parsePrivateName();
        }
      case 33:
        {
          return this.parseTopicReferenceThenEqualsSign(54, "%");
        }
      case 32:
        {
          return this.parseTopicReferenceThenEqualsSign(44, "^");
        }
      case 37:
      case 38:
        {
          return this.parseTopicReference("hack");
        }
      case 44:
      case 54:
      case 27:
        {
          const pipeProposal = this.getPluginOption("pipelineOperator", "proposal");
          if (pipeProposal) {
            return this.parseTopicReference(pipeProposal);
          }
          this.unexpected();
          break;
        }
      case 47:
        {
          const lookaheadCh = this.input.codePointAt(this.nextTokenStart());
          if ((0, _identifier.isIdentifierStart)(lookaheadCh) || lookaheadCh === 62) {
            this.expectOnePlugin(["jsx", "flow", "typescript"]);
          } else {
            this.unexpected();
          }
          break;
        }
      default:
        if ((0, _types.tokenIsIdentifier)(type)) {
          if (this.isContextual(125) && this.lookaheadInLineCharCode() === 123) {
            return this.parseModuleExpression();
          }
          const canBeArrow = this.state.potentialArrowAt === this.state.start;
          const containsEsc = this.state.containsEsc;
          const id = this.parseIdentifier();
          if (!containsEsc && id.name === "async" && !this.canInsertSemicolon()) {
            const {
              type
            } = this.state;
            if (type === 68) {
              this.resetPreviousNodeTrailingComments(id);
              this.next();
              return this.parseAsyncFunctionExpression(this.startNodeAtNode(id));
            } else if ((0, _types.tokenIsIdentifier)(type)) {
              if (this.lookaheadCharCode() === 61) {
                return this.parseAsyncArrowUnaryFunction(this.startNodeAtNode(id));
              } else {
                return id;
              }
            } else if (type === 90) {
              this.resetPreviousNodeTrailingComments(id);
              return this.parseDo(this.startNodeAtNode(id), true);
            }
          }
          if (canBeArrow && this.match(19) && !this.canInsertSemicolon()) {
            this.next();
            return this.parseArrowExpression(this.startNodeAtNode(id), [id], false);
          }
          return id;
        } else {
          this.unexpected();
        }
    }
  }
  parseTopicReferenceThenEqualsSign(topicTokenType, topicTokenValue) {
    const pipeProposal = this.getPluginOption("pipelineOperator", "proposal");
    if (pipeProposal) {
      this.state.type = topicTokenType;
      this.state.value = topicTokenValue;
      this.state.pos--;
      this.state.end--;
      this.state.endLoc = (0, _location.createPositionWithColumnOffset)(this.state.endLoc, -1);
      return this.parseTopicReference(pipeProposal);
    } else {
      this.unexpected();
    }
  }
  parseTopicReference(pipeProposal) {
    const node = this.startNode();
    const startLoc = this.state.startLoc;
    const tokenType = this.state.type;
    this.next();
    return this.finishTopicReference(node, startLoc, pipeProposal, tokenType);
  }
  finishTopicReference(node, startLoc, pipeProposal, tokenType) {
    if (this.testTopicReferenceConfiguration(pipeProposal, startLoc, tokenType)) {
      const nodeType = pipeProposal === "smart" ? "PipelinePrimaryTopicReference" : "TopicReference";
      if (!this.topicReferenceIsAllowedInCurrentContext()) {
        this.raise(pipeProposal === "smart" ? _parseError.Errors.PrimaryTopicNotAllowed : _parseError.Errors.PipeTopicUnbound, {
          at: startLoc
        });
      }
      this.registerTopicReference();
      return this.finishNode(node, nodeType);
    } else {
      throw this.raise(_parseError.Errors.PipeTopicUnconfiguredToken, {
        at: startLoc,
        token: (0, _types.tokenLabelName)(tokenType)
      });
    }
  }
  testTopicReferenceConfiguration(pipeProposal, startLoc, tokenType) {
    switch (pipeProposal) {
      case "hack":
        {
          return this.hasPlugin(["pipelineOperator", {
            topicToken: (0, _types.tokenLabelName)(tokenType)
          }]);
        }
      case "smart":
        return tokenType === 27;
      default:
        throw this.raise(_parseError.Errors.PipeTopicRequiresHackPipes, {
          at: startLoc
        });
    }
  }
  parseAsyncArrowUnaryFunction(node) {
    this.prodParam.enter((0, _productionParameter.functionFlags)(true, this.prodParam.hasYield));
    const params = [this.parseIdentifier()];
    this.prodParam.exit();
    if (this.hasPrecedingLineBreak()) {
      this.raise(_parseError.Errors.LineTerminatorBeforeArrow, {
        at: this.state.curPosition()
      });
    }
    this.expect(19);
    return this.parseArrowExpression(node, params, true);
  }
  parseDo(node, isAsync) {
    this.expectPlugin("doExpressions");
    if (isAsync) {
      this.expectPlugin("asyncDoExpressions");
    }
    node.async = isAsync;
    this.next();
    const oldLabels = this.state.labels;
    this.state.labels = [];
    if (isAsync) {
      this.prodParam.enter(_productionParameter.PARAM_AWAIT);
      node.body = this.parseBlock();
      this.prodParam.exit();
    } else {
      node.body = this.parseBlock();
    }
    this.state.labels = oldLabels;
    return this.finishNode(node, "DoExpression");
  }
  parseSuper() {
    const node = this.startNode();
    this.next();
    if (this.match(10) && !this.scope.allowDirectSuper && !this.options.allowSuperOutsideMethod) {
      this.raise(_parseError.Errors.SuperNotAllowed, {
        at: node
      });
    } else if (!this.scope.allowSuper && !this.options.allowSuperOutsideMethod) {
      this.raise(_parseError.Errors.UnexpectedSuper, {
        at: node
      });
    }
    if (!this.match(10) && !this.match(0) && !this.match(16)) {
      this.raise(_parseError.Errors.UnsupportedSuper, {
        at: node
      });
    }
    return this.finishNode(node, "Super");
  }
  parsePrivateName() {
    const node = this.startNode();
    const id = this.startNodeAt((0, _location.createPositionWithColumnOffset)(this.state.startLoc, 1));
    const name = this.state.value;
    this.next();
    node.id = this.createIdentifier(id, name);
    return this.finishNode(node, "PrivateName");
  }
  parseFunctionOrFunctionSent() {
    const node = this.startNode();
    this.next();
    if (this.prodParam.hasYield && this.match(16)) {
      const meta = this.createIdentifier(this.startNodeAtNode(node), "function");
      this.next();
      if (this.match(102)) {
        this.expectPlugin("functionSent");
      } else if (!this.hasPlugin("functionSent")) {
        this.unexpected();
      }
      return this.parseMetaProperty(node, meta, "sent");
    }
    return this.parseFunction(node);
  }
  parseMetaProperty(node, meta, propertyName) {
    node.meta = meta;
    const containsEsc = this.state.containsEsc;
    node.property = this.parseIdentifier(true);
    if (node.property.name !== propertyName || containsEsc) {
      this.raise(_parseError.Errors.UnsupportedMetaProperty, {
        at: node.property,
        target: meta.name,
        onlyValidPropertyName: propertyName
      });
    }
    return this.finishNode(node, "MetaProperty");
  }
  parseImportMetaProperty(node) {
    const id = this.createIdentifier(this.startNodeAtNode(node), "import");
    this.next();
    if (this.isContextual(100)) {
      if (!this.inModule) {
        this.raise(_parseError.Errors.ImportMetaOutsideModule, {
          at: id
        });
      }
      this.sawUnambiguousESM = true;
    }
    return this.parseMetaProperty(node, id, "meta");
  }
  parseLiteralAtNode(value, type, node) {
    this.addExtra(node, "rawValue", value);
    this.addExtra(node, "raw", this.input.slice(node.start, this.state.end));
    node.value = value;
    this.next();
    return this.finishNode(node, type);
  }
  parseLiteral(value, type) {
    const node = this.startNode();
    return this.parseLiteralAtNode(value, type, node);
  }
  parseStringLiteral(value) {
    return this.parseLiteral(value, "StringLiteral");
  }
  parseNumericLiteral(value) {
    return this.parseLiteral(value, "NumericLiteral");
  }
  parseBigIntLiteral(value) {
    return this.parseLiteral(value, "BigIntLiteral");
  }
  parseDecimalLiteral(value) {
    return this.parseLiteral(value, "DecimalLiteral");
  }
  parseRegExpLiteral(value) {
    const node = this.parseLiteral(value.value, "RegExpLiteral");
    node.pattern = value.pattern;
    node.flags = value.flags;
    return node;
  }
  parseBooleanLiteral(value) {
    const node = this.startNode();
    node.value = value;
    this.next();
    return this.finishNode(node, "BooleanLiteral");
  }
  parseNullLiteral() {
    const node = this.startNode();
    this.next();
    return this.finishNode(node, "NullLiteral");
  }
  parseParenAndDistinguishExpression(canBeArrow) {
    const startLoc = this.state.startLoc;
    let val;
    this.next();
    this.expressionScope.enter((0, _expressionScope.newArrowHeadScope)());
    const oldMaybeInArrowParameters = this.state.maybeInArrowParameters;
    const oldInFSharpPipelineDirectBody = this.state.inFSharpPipelineDirectBody;
    this.state.maybeInArrowParameters = true;
    this.state.inFSharpPipelineDirectBody = false;
    const innerStartLoc = this.state.startLoc;
    const exprList = [];
    const refExpressionErrors = new _util.ExpressionErrors();
    let first = true;
    let spreadStartLoc;
    let optionalCommaStartLoc;
    while (!this.match(11)) {
      if (first) {
        first = false;
      } else {
        this.expect(12, refExpressionErrors.optionalParametersLoc === null ? null : refExpressionErrors.optionalParametersLoc);
        if (this.match(11)) {
          optionalCommaStartLoc = this.state.startLoc;
          break;
        }
      }
      if (this.match(21)) {
        const spreadNodeStartLoc = this.state.startLoc;
        spreadStartLoc = this.state.startLoc;
        exprList.push(this.parseParenItem(this.parseRestBinding(), spreadNodeStartLoc));
        if (!this.checkCommaAfterRest(41)) {
          break;
        }
      } else {
        exprList.push(this.parseMaybeAssignAllowIn(refExpressionErrors, this.parseParenItem));
      }
    }
    const innerEndLoc = this.state.lastTokEndLoc;
    this.expect(11);
    this.state.maybeInArrowParameters = oldMaybeInArrowParameters;
    this.state.inFSharpPipelineDirectBody = oldInFSharpPipelineDirectBody;
    let arrowNode = this.startNodeAt(startLoc);
    if (canBeArrow && this.shouldParseArrow(exprList) && (arrowNode = this.parseArrow(arrowNode))) {
      this.checkDestructuringPrivate(refExpressionErrors);
      this.expressionScope.validateAsPattern();
      this.expressionScope.exit();
      this.parseArrowExpression(arrowNode, exprList, false);
      return arrowNode;
    }
    this.expressionScope.exit();
    if (!exprList.length) {
      this.unexpected(this.state.lastTokStartLoc);
    }
    if (optionalCommaStartLoc) this.unexpected(optionalCommaStartLoc);
    if (spreadStartLoc) this.unexpected(spreadStartLoc);
    this.checkExpressionErrors(refExpressionErrors, true);
    this.toReferencedListDeep(exprList, true);
    if (exprList.length > 1) {
      val = this.startNodeAt(innerStartLoc);
      val.expressions = exprList;
      this.finishNode(val, "SequenceExpression");
      this.resetEndLocation(val, innerEndLoc);
    } else {
      val = exprList[0];
    }
    return this.wrapParenthesis(startLoc, val);
  }
  wrapParenthesis(startLoc, expression) {
    if (!this.options.createParenthesizedExpressions) {
      this.addExtra(expression, "parenthesized", true);
      this.addExtra(expression, "parenStart", startLoc.index);
      this.takeSurroundingComments(expression, startLoc.index, this.state.lastTokEndLoc.index);
      return expression;
    }
    const parenExpression = this.startNodeAt(startLoc);
    parenExpression.expression = expression;
    return this.finishNode(parenExpression, "ParenthesizedExpression");
  }
  shouldParseArrow(params) {
    return !this.canInsertSemicolon();
  }
  parseArrow(node) {
    if (this.eat(19)) {
      return node;
    }
  }
  parseParenItem(node, startLoc) {
    return node;
  }
  parseNewOrNewTarget() {
    const node = this.startNode();
    this.next();
    if (this.match(16)) {
      const meta = this.createIdentifier(this.startNodeAtNode(node), "new");
      this.next();
      const metaProp = this.parseMetaProperty(node, meta, "target");
      if (!this.scope.inNonArrowFunction && !this.scope.inClass && !this.options.allowNewTargetOutsideFunction) {
        this.raise(_parseError.Errors.UnexpectedNewTarget, {
          at: metaProp
        });
      }
      return metaProp;
    }
    return this.parseNew(node);
  }
  parseNew(node) {
    this.parseNewCallee(node);
    if (this.eat(10)) {
      const args = this.parseExprList(11);
      this.toReferencedList(args);
      node.arguments = args;
    } else {
      node.arguments = [];
    }
    return this.finishNode(node, "NewExpression");
  }
  parseNewCallee(node) {
    node.callee = this.parseNoCallExpr();
    if (node.callee.type === "Import") {
      this.raise(_parseError.Errors.ImportCallNotNewExpression, {
        at: node.callee
      });
    }
  }
  parseTemplateElement(isTagged) {
    const {
      start,
      startLoc,
      end,
      value
    } = this.state;
    const elemStart = start + 1;
    const elem = this.startNodeAt((0, _location.createPositionWithColumnOffset)(startLoc, 1));
    if (value === null) {
      if (!isTagged) {
        this.raise(_parseError.Errors.InvalidEscapeSequenceTemplate, {
          at: (0, _location.createPositionWithColumnOffset)(this.state.firstInvalidTemplateEscapePos, 1)
        });
      }
    }
    const isTail = this.match(24);
    const endOffset = isTail ? -1 : -2;
    const elemEnd = end + endOffset;
    elem.value = {
      raw: this.input.slice(elemStart, elemEnd).replace(/\r\n?/g, "\n"),
      cooked: value === null ? null : value.slice(1, endOffset)
    };
    elem.tail = isTail;
    this.next();
    const finishedNode = this.finishNode(elem, "TemplateElement");
    this.resetEndLocation(finishedNode, (0, _location.createPositionWithColumnOffset)(this.state.lastTokEndLoc, endOffset));
    return finishedNode;
  }
  parseTemplate(isTagged) {
    const node = this.startNode();
    node.expressions = [];
    let curElt = this.parseTemplateElement(isTagged);
    node.quasis = [curElt];
    while (!curElt.tail) {
      node.expressions.push(this.parseTemplateSubstitution());
      this.readTemplateContinuation();
      node.quasis.push(curElt = this.parseTemplateElement(isTagged));
    }
    return this.finishNode(node, "TemplateLiteral");
  }
  parseTemplateSubstitution() {
    return this.parseExpression();
  }
  parseObjectLike(close, isPattern, isRecord, refExpressionErrors) {
    if (isRecord) {
      this.expectPlugin("recordAndTuple");
    }
    const oldInFSharpPipelineDirectBody = this.state.inFSharpPipelineDirectBody;
    this.state.inFSharpPipelineDirectBody = false;
    const propHash = Object.create(null);
    let first = true;
    const node = this.startNode();
    node.properties = [];
    this.next();
    while (!this.match(close)) {
      if (first) {
        first = false;
      } else {
        this.expect(12);
        if (this.match(close)) {
          this.addTrailingCommaExtraToNode(node);
          break;
        }
      }
      let prop;
      if (isPattern) {
        prop = this.parseBindingProperty();
      } else {
        prop = this.parsePropertyDefinition(refExpressionErrors);
        this.checkProto(prop, isRecord, propHash, refExpressionErrors);
      }
      if (isRecord && !this.isObjectProperty(prop) && prop.type !== "SpreadElement") {
        this.raise(_parseError.Errors.InvalidRecordProperty, {
          at: prop
        });
      }
      if (prop.shorthand) {
        this.addExtra(prop, "shorthand", true);
      }
      node.properties.push(prop);
    }
    this.next();
    this.state.inFSharpPipelineDirectBody = oldInFSharpPipelineDirectBody;
    let type = "ObjectExpression";
    if (isPattern) {
      type = "ObjectPattern";
    } else if (isRecord) {
      type = "RecordExpression";
    }
    return this.finishNode(node, type);
  }
  addTrailingCommaExtraToNode(node) {
    this.addExtra(node, "trailingComma", this.state.lastTokStart);
    this.addExtra(node, "trailingCommaLoc", this.state.lastTokStartLoc, false);
  }
  maybeAsyncOrAccessorProp(prop) {
    return !prop.computed && prop.key.type === "Identifier" && (this.isLiteralPropertyName() || this.match(0) || this.match(55));
  }
  parsePropertyDefinition(refExpressionErrors) {
    let decorators = [];
    if (this.match(26)) {
      if (this.hasPlugin("decorators")) {
        this.raise(_parseError.Errors.UnsupportedPropertyDecorator, {
          at: this.state.startLoc
        });
      }
      while (this.match(26)) {
        decorators.push(this.parseDecorator());
      }
    }
    const prop = this.startNode();
    let isAsync = false;
    let isAccessor = false;
    let startLoc;
    if (this.match(21)) {
      if (decorators.length) this.unexpected();
      return this.parseSpread();
    }
    if (decorators.length) {
      prop.decorators = decorators;
      decorators = [];
    }
    prop.method = false;
    if (refExpressionErrors) {
      startLoc = this.state.startLoc;
    }
    let isGenerator = this.eat(55);
    this.parsePropertyNamePrefixOperator(prop);
    const containsEsc = this.state.containsEsc;
    const key = this.parsePropertyName(prop, refExpressionErrors);
    if (!isGenerator && !containsEsc && this.maybeAsyncOrAccessorProp(prop)) {
      const keyName = key.name;
      if (keyName === "async" && !this.hasPrecedingLineBreak()) {
        isAsync = true;
        this.resetPreviousNodeTrailingComments(key);
        isGenerator = this.eat(55);
        this.parsePropertyName(prop);
      }
      if (keyName === "get" || keyName === "set") {
        isAccessor = true;
        this.resetPreviousNodeTrailingComments(key);
        prop.kind = keyName;
        if (this.match(55)) {
          isGenerator = true;
          this.raise(_parseError.Errors.AccessorIsGenerator, {
            at: this.state.curPosition(),
            kind: keyName
          });
          this.next();
        }
        this.parsePropertyName(prop);
      }
    }
    return this.parseObjPropValue(prop, startLoc, isGenerator, isAsync, false, isAccessor, refExpressionErrors);
  }
  getGetterSetterExpectedParamCount(method) {
    return method.kind === "get" ? 0 : 1;
  }
  getObjectOrClassMethodParams(method) {
    return method.params;
  }
  checkGetterSetterParams(method) {
    var _params;
    const paramCount = this.getGetterSetterExpectedParamCount(method);
    const params = this.getObjectOrClassMethodParams(method);
    if (params.length !== paramCount) {
      this.raise(method.kind === "get" ? _parseError.Errors.BadGetterArity : _parseError.Errors.BadSetterArity, {
        at: method
      });
    }
    if (method.kind === "set" && ((_params = params[params.length - 1]) == null ? void 0 : _params.type) === "RestElement") {
      this.raise(_parseError.Errors.BadSetterRestParameter, {
        at: method
      });
    }
  }
  parseObjectMethod(prop, isGenerator, isAsync, isPattern, isAccessor) {
    if (isAccessor) {
      const finishedProp = this.parseMethod(prop, isGenerator, false, false, false, "ObjectMethod");
      this.checkGetterSetterParams(finishedProp);
      return finishedProp;
    }
    if (isAsync || isGenerator || this.match(10)) {
      if (isPattern) this.unexpected();
      prop.kind = "method";
      prop.method = true;
      return this.parseMethod(prop, isGenerator, isAsync, false, false, "ObjectMethod");
    }
  }
  parseObjectProperty(prop, startLoc, isPattern, refExpressionErrors) {
    prop.shorthand = false;
    if (this.eat(14)) {
      prop.value = isPattern ? this.parseMaybeDefault(this.state.startLoc) : this.parseMaybeAssignAllowIn(refExpressionErrors);
      return this.finishNode(prop, "ObjectProperty");
    }
    if (!prop.computed && prop.key.type === "Identifier") {
      this.checkReservedWord(prop.key.name, prop.key.loc.start, true, false);
      if (isPattern) {
        prop.value = this.parseMaybeDefault(startLoc, (0, _node.cloneIdentifier)(prop.key));
      } else if (this.match(29)) {
        const shorthandAssignLoc = this.state.startLoc;
        if (refExpressionErrors != null) {
          if (refExpressionErrors.shorthandAssignLoc === null) {
            refExpressionErrors.shorthandAssignLoc = shorthandAssignLoc;
          }
        } else {
          this.raise(_parseError.Errors.InvalidCoverInitializedName, {
            at: shorthandAssignLoc
          });
        }
        prop.value = this.parseMaybeDefault(startLoc, (0, _node.cloneIdentifier)(prop.key));
      } else {
        prop.value = (0, _node.cloneIdentifier)(prop.key);
      }
      prop.shorthand = true;
      return this.finishNode(prop, "ObjectProperty");
    }
  }
  parseObjPropValue(prop, startLoc, isGenerator, isAsync, isPattern, isAccessor, refExpressionErrors) {
    const node = this.parseObjectMethod(prop, isGenerator, isAsync, isPattern, isAccessor) || this.parseObjectProperty(prop, startLoc, isPattern, refExpressionErrors);
    if (!node) this.unexpected();
    return node;
  }
  parsePropertyName(prop, refExpressionErrors) {
    if (this.eat(0)) {
      prop.computed = true;
      prop.key = this.parseMaybeAssignAllowIn();
      this.expect(3);
    } else {
      const {
        type,
        value
      } = this.state;
      let key;
      if ((0, _types.tokenIsKeywordOrIdentifier)(type)) {
        key = this.parseIdentifier(true);
      } else {
        switch (type) {
          case 132:
            key = this.parseNumericLiteral(value);
            break;
          case 131:
            key = this.parseStringLiteral(value);
            break;
          case 133:
            key = this.parseBigIntLiteral(value);
            break;
          case 134:
            key = this.parseDecimalLiteral(value);
            break;
          case 136:
            {
              const privateKeyLoc = this.state.startLoc;
              if (refExpressionErrors != null) {
                if (refExpressionErrors.privateKeyLoc === null) {
                  refExpressionErrors.privateKeyLoc = privateKeyLoc;
                }
              } else {
                this.raise(_parseError.Errors.UnexpectedPrivateField, {
                  at: privateKeyLoc
                });
              }
              key = this.parsePrivateName();
              break;
            }
          default:
            this.unexpected();
        }
      }
      prop.key = key;
      if (type !== 136) {
        prop.computed = false;
      }
    }
    return prop.key;
  }
  initFunction(node, isAsync) {
    node.id = null;
    node.generator = false;
    node.async = isAsync;
  }
  parseMethod(node, isGenerator, isAsync, isConstructor, allowDirectSuper, type, inClassScope = false) {
    this.initFunction(node, isAsync);
    node.generator = isGenerator;
    this.scope.enter(_scopeflags.ScopeFlag.FUNCTION | _scopeflags.ScopeFlag.SUPER | (inClassScope ? _scopeflags.ScopeFlag.CLASS : 0) | (allowDirectSuper ? _scopeflags.ScopeFlag.DIRECT_SUPER : 0));
    this.prodParam.enter((0, _productionParameter.functionFlags)(isAsync, node.generator));
    this.parseFunctionParams(node, isConstructor);
    const finishedNode = this.parseFunctionBodyAndFinish(node, type, true);
    this.prodParam.exit();
    this.scope.exit();
    return finishedNode;
  }
  parseArrayLike(close, canBePattern, isTuple, refExpressionErrors) {
    if (isTuple) {
      this.expectPlugin("recordAndTuple");
    }
    const oldInFSharpPipelineDirectBody = this.state.inFSharpPipelineDirectBody;
    this.state.inFSharpPipelineDirectBody = false;
    const node = this.startNode();
    this.next();
    node.elements = this.parseExprList(close, !isTuple, refExpressionErrors, node);
    this.state.inFSharpPipelineDirectBody = oldInFSharpPipelineDirectBody;
    return this.finishNode(node, isTuple ? "TupleExpression" : "ArrayExpression");
  }
  parseArrowExpression(node, params, isAsync, trailingCommaLoc) {
    this.scope.enter(_scopeflags.ScopeFlag.FUNCTION | _scopeflags.ScopeFlag.ARROW);
    let flags = (0, _productionParameter.functionFlags)(isAsync, false);
    if (!this.match(5) && this.prodParam.hasIn) {
      flags |= _productionParameter.PARAM_IN;
    }
    this.prodParam.enter(flags);
    this.initFunction(node, isAsync);
    const oldMaybeInArrowParameters = this.state.maybeInArrowParameters;
    if (params) {
      this.state.maybeInArrowParameters = true;
      this.setArrowFunctionParameters(node, params, trailingCommaLoc);
    }
    this.state.maybeInArrowParameters = false;
    this.parseFunctionBody(node, true);
    this.prodParam.exit();
    this.scope.exit();
    this.state.maybeInArrowParameters = oldMaybeInArrowParameters;
    return this.finishNode(node, "ArrowFunctionExpression");
  }
  setArrowFunctionParameters(node, params, trailingCommaLoc) {
    this.toAssignableList(params, trailingCommaLoc, false);
    node.params = params;
  }
  parseFunctionBodyAndFinish(node, type, isMethod = false) {
    this.parseFunctionBody(node, false, isMethod);
    return this.finishNode(node, type);
  }
  parseFunctionBody(node, allowExpression, isMethod = false) {
    const isExpression = allowExpression && !this.match(5);
    this.expressionScope.enter((0, _expressionScope.newExpressionScope)());
    if (isExpression) {
      node.body = this.parseMaybeAssign();
      this.checkParams(node, false, allowExpression, false);
    } else {
      const oldStrict = this.state.strict;
      const oldLabels = this.state.labels;
      this.state.labels = [];
      this.prodParam.enter(this.prodParam.currentFlags() | _productionParameter.PARAM_RETURN);
      node.body = this.parseBlock(true, false, hasStrictModeDirective => {
        const nonSimple = !this.isSimpleParamList(node.params);
        if (hasStrictModeDirective && nonSimple) {
          this.raise(_parseError.Errors.IllegalLanguageModeDirective, {
            at: (node.kind === "method" || node.kind === "constructor") && !!node.key ? node.key.loc.end : node
          });
        }
        const strictModeChanged = !oldStrict && this.state.strict;
        this.checkParams(node, !this.state.strict && !allowExpression && !isMethod && !nonSimple, allowExpression, strictModeChanged);
        if (this.state.strict && node.id) {
          this.checkIdentifier(node.id, _scopeflags.BindingFlag.TYPE_OUTSIDE, strictModeChanged);
        }
      });
      this.prodParam.exit();
      this.state.labels = oldLabels;
    }
    this.expressionScope.exit();
  }
  isSimpleParameter(node) {
    return node.type === "Identifier";
  }
  isSimpleParamList(params) {
    for (let i = 0, len = params.length; i < len; i++) {
      if (!this.isSimpleParameter(params[i])) return false;
    }
    return true;
  }
  checkParams(node, allowDuplicates, isArrowFunction, strictModeChanged = true) {
    const checkClashes = !allowDuplicates && new Set();
    const formalParameters = {
      type: "FormalParameters"
    };
    for (const param of node.params) {
      this.checkLVal(param, {
        in: formalParameters,
        binding: _scopeflags.BindingFlag.TYPE_VAR,
        checkClashes,
        strictModeChanged
      });
    }
  }
  parseExprList(close, allowEmpty, refExpressionErrors, nodeForExtra) {
    const elts = [];
    let first = true;
    while (!this.eat(close)) {
      if (first) {
        first = false;
      } else {
        this.expect(12);
        if (this.match(close)) {
          if (nodeForExtra) {
            this.addTrailingCommaExtraToNode(nodeForExtra);
          }
          this.next();
          break;
        }
      }
      elts.push(this.parseExprListItem(allowEmpty, refExpressionErrors));
    }
    return elts;
  }
  parseExprListItem(allowEmpty, refExpressionErrors, allowPlaceholder) {
    let elt;
    if (this.match(12)) {
      if (!allowEmpty) {
        this.raise(_parseError.Errors.UnexpectedToken, {
          at: this.state.curPosition(),
          unexpected: ","
        });
      }
      elt = null;
    } else if (this.match(21)) {
      const spreadNodeStartLoc = this.state.startLoc;
      elt = this.parseParenItem(this.parseSpread(refExpressionErrors), spreadNodeStartLoc);
    } else if (this.match(17)) {
      this.expectPlugin("partialApplication");
      if (!allowPlaceholder) {
        this.raise(_parseError.Errors.UnexpectedArgumentPlaceholder, {
          at: this.state.startLoc
        });
      }
      const node = this.startNode();
      this.next();
      elt = this.finishNode(node, "ArgumentPlaceholder");
    } else {
      elt = this.parseMaybeAssignAllowIn(refExpressionErrors, this.parseParenItem);
    }
    return elt;
  }
  parseIdentifier(liberal) {
    const node = this.startNode();
    const name = this.parseIdentifierName(liberal);
    return this.createIdentifier(node, name);
  }
  createIdentifier(node, name) {
    node.name = name;
    node.loc.identifierName = name;
    return this.finishNode(node, "Identifier");
  }
  parseIdentifierName(liberal) {
    let name;
    const {
      startLoc,
      type
    } = this.state;
    if ((0, _types.tokenIsKeywordOrIdentifier)(type)) {
      name = this.state.value;
    } else {
      this.unexpected();
    }
    const tokenIsKeyword = (0, _types.tokenKeywordOrIdentifierIsKeyword)(type);
    if (liberal) {
      if (tokenIsKeyword) {
        this.replaceToken(130);
      }
    } else {
      this.checkReservedWord(name, startLoc, tokenIsKeyword, false);
    }
    this.next();
    return name;
  }
  checkReservedWord(word, startLoc, checkKeywords, isBinding) {
    if (word.length > 10) {
      return;
    }
    if (!(0, _identifier.canBeReservedWord)(word)) {
      return;
    }
    if (checkKeywords && (0, _identifier.isKeyword)(word)) {
      this.raise(_parseError.Errors.UnexpectedKeyword, {
        at: startLoc,
        keyword: word
      });
      return;
    }
    const reservedTest = !this.state.strict ? _identifier.isReservedWord : isBinding ? _identifier.isStrictBindReservedWord : _identifier.isStrictReservedWord;
    if (reservedTest(word, this.inModule)) {
      this.raise(_parseError.Errors.UnexpectedReservedWord, {
        at: startLoc,
        reservedWord: word
      });
      return;
    } else if (word === "yield") {
      if (this.prodParam.hasYield) {
        this.raise(_parseError.Errors.YieldBindingIdentifier, {
          at: startLoc
        });
        return;
      }
    } else if (word === "await") {
      if (this.prodParam.hasAwait) {
        this.raise(_parseError.Errors.AwaitBindingIdentifier, {
          at: startLoc
        });
        return;
      }
      if (this.scope.inStaticBlock) {
        this.raise(_parseError.Errors.AwaitBindingIdentifierInStaticBlock, {
          at: startLoc
        });
        return;
      }
      this.expressionScope.recordAsyncArrowParametersError({
        at: startLoc
      });
    } else if (word === "arguments") {
      if (this.scope.inClassAndNotInNonArrowFunction) {
        this.raise(_parseError.Errors.ArgumentsInClass, {
          at: startLoc
        });
        return;
      }
    }
  }
  isAwaitAllowed() {
    if (this.prodParam.hasAwait) return true;
    if (this.options.allowAwaitOutsideFunction && !this.scope.inFunction) {
      return true;
    }
    return false;
  }
  parseAwait(startLoc) {
    const node = this.startNodeAt(startLoc);
    this.expressionScope.recordParameterInitializerError(_parseError.Errors.AwaitExpressionFormalParameter, {
      at: node
    });
    if (this.eat(55)) {
      this.raise(_parseError.Errors.ObsoleteAwaitStar, {
        at: node
      });
    }
    if (!this.scope.inFunction && !this.options.allowAwaitOutsideFunction) {
      if (this.isAmbiguousAwait()) {
        this.ambiguousScriptDifferentAst = true;
      } else {
        this.sawUnambiguousESM = true;
      }
    }
    if (!this.state.soloAwait) {
      node.argument = this.parseMaybeUnary(null, true);
    }
    return this.finishNode(node, "AwaitExpression");
  }
  isAmbiguousAwait() {
    if (this.hasPrecedingLineBreak()) return true;
    const {
      type
    } = this.state;
    return type === 53 || type === 10 || type === 0 || (0, _types.tokenIsTemplate)(type) || type === 101 && !this.state.containsEsc || type === 135 || type === 56 || this.hasPlugin("v8intrinsic") && type === 54;
  }
  parseYield() {
    const node = this.startNode();
    this.expressionScope.recordParameterInitializerError(_parseError.Errors.YieldInParameter, {
      at: node
    });
    this.next();
    let delegating = false;
    let argument = null;
    if (!this.hasPrecedingLineBreak()) {
      delegating = this.eat(55);
      switch (this.state.type) {
        case 13:
        case 137:
        case 8:
        case 11:
        case 3:
        case 9:
        case 14:
        case 12:
          if (!delegating) break;
        default:
          argument = this.parseMaybeAssign();
      }
    }
    node.delegate = delegating;
    node.argument = argument;
    return this.finishNode(node, "YieldExpression");
  }
  checkPipelineAtInfixOperator(left, leftStartLoc) {
    if (this.hasPlugin(["pipelineOperator", {
      proposal: "smart"
    }])) {
      if (left.type === "SequenceExpression") {
        this.raise(_parseError.Errors.PipelineHeadSequenceExpression, {
          at: leftStartLoc
        });
      }
    }
  }
  parseSmartPipelineBodyInStyle(childExpr, startLoc) {
    if (this.isSimpleReference(childExpr)) {
      const bodyNode = this.startNodeAt(startLoc);
      bodyNode.callee = childExpr;
      return this.finishNode(bodyNode, "PipelineBareFunction");
    } else {
      const bodyNode = this.startNodeAt(startLoc);
      this.checkSmartPipeTopicBodyEarlyErrors(startLoc);
      bodyNode.expression = childExpr;
      return this.finishNode(bodyNode, "PipelineTopicExpression");
    }
  }
  isSimpleReference(expression) {
    switch (expression.type) {
      case "MemberExpression":
        return !expression.computed && this.isSimpleReference(expression.object);
      case "Identifier":
        return true;
      default:
        return false;
    }
  }
  checkSmartPipeTopicBodyEarlyErrors(startLoc) {
    if (this.match(19)) {
      throw this.raise(_parseError.Errors.PipelineBodyNoArrow, {
        at: this.state.startLoc
      });
    }
    if (!this.topicReferenceWasUsedInCurrentContext()) {
      this.raise(_parseError.Errors.PipelineTopicUnused, {
        at: startLoc
      });
    }
  }
  withTopicBindingContext(callback) {
    const outerContextTopicState = this.state.topicContext;
    this.state.topicContext = {
      maxNumOfResolvableTopics: 1,
      maxTopicIndex: null
    };
    try {
      return callback();
    } finally {
      this.state.topicContext = outerContextTopicState;
    }
  }
  withSmartMixTopicForbiddingContext(callback) {
    if (this.hasPlugin(["pipelineOperator", {
      proposal: "smart"
    }])) {
      const outerContextTopicState = this.state.topicContext;
      this.state.topicContext = {
        maxNumOfResolvableTopics: 0,
        maxTopicIndex: null
      };
      try {
        return callback();
      } finally {
        this.state.topicContext = outerContextTopicState;
      }
    } else {
      return callback();
    }
  }
  withSoloAwaitPermittingContext(callback) {
    const outerContextSoloAwaitState = this.state.soloAwait;
    this.state.soloAwait = true;
    try {
      return callback();
    } finally {
      this.state.soloAwait = outerContextSoloAwaitState;
    }
  }
  allowInAnd(callback) {
    const flags = this.prodParam.currentFlags();
    const prodParamToSet = _productionParameter.PARAM_IN & ~flags;
    if (prodParamToSet) {
      this.prodParam.enter(flags | _productionParameter.PARAM_IN);
      try {
        return callback();
      } finally {
        this.prodParam.exit();
      }
    }
    return callback();
  }
  disallowInAnd(callback) {
    const flags = this.prodParam.currentFlags();
    const prodParamToClear = _productionParameter.PARAM_IN & flags;
    if (prodParamToClear) {
      this.prodParam.enter(flags & ~_productionParameter.PARAM_IN);
      try {
        return callback();
      } finally {
        this.prodParam.exit();
      }
    }
    return callback();
  }
  registerTopicReference() {
    this.state.topicContext.maxTopicIndex = 0;
  }
  topicReferenceIsAllowedInCurrentContext() {
    return this.state.topicContext.maxNumOfResolvableTopics >= 1;
  }
  topicReferenceWasUsedInCurrentContext() {
    return this.state.topicContext.maxTopicIndex != null && this.state.topicContext.maxTopicIndex >= 0;
  }
  parseFSharpPipelineBody(prec) {
    const startLoc = this.state.startLoc;
    this.state.potentialArrowAt = this.state.start;
    const oldInFSharpPipelineDirectBody = this.state.inFSharpPipelineDirectBody;
    this.state.inFSharpPipelineDirectBody = true;
    const ret = this.parseExprOp(this.parseMaybeUnaryOrPrivate(), startLoc, prec);
    this.state.inFSharpPipelineDirectBody = oldInFSharpPipelineDirectBody;
    return ret;
  }
  parseModuleExpression() {
    this.expectPlugin("moduleBlocks");
    const node = this.startNode();
    this.next();
    if (!this.match(5)) {
      this.unexpected(null, 5);
    }
    const program = this.startNodeAt(this.state.endLoc);
    this.next();
    const revertScopes = this.initializeScopes(true);
    this.enterInitialScopes();
    try {
      node.body = this.parseProgram(program, 8, "module");
    } finally {
      revertScopes();
    }
    return this.finishNode(node, "ModuleExpression");
  }
  parsePropertyNamePrefixOperator(prop) {}
}
exports.default = ExpressionParser;

//# sourceMappingURL=expression.js.map
