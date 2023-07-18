"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = exports.ParseStatementFlag = exports.ParseFunctionFlag = void 0;
var _types = require("../tokenizer/types");
var _expression = require("./expression");
var _parseError = require("../parse-error");
var _identifier = require("../util/identifier");
var _scopeflags = require("../util/scopeflags");
var _util = require("./util");
var _productionParameter = require("../util/production-parameter");
var _expressionScope = require("../util/expression-scope");
var _tokenizer = require("../tokenizer");
var _location = require("../util/location");
var _node = require("./node");
var _lval = require("./lval");
const loopLabel = {
    kind: "loop"
  },
  switchLabel = {
    kind: "switch"
  };
var ParseFunctionFlag = {
  Expression: 0,
  Declaration: 1,
  HangingDeclaration: 2,
  NullableId: 4,
  Async: 8
};
exports.ParseFunctionFlag = ParseFunctionFlag;
var ParseStatementFlag = {
  StatementOnly: 0,
  AllowImportExport: 1,
  AllowDeclaration: 2,
  AllowFunctionDeclaration: 4,
  AllowLabeledFunction: 8
};
exports.ParseStatementFlag = ParseStatementFlag;
const loneSurrogate = /[\uD800-\uDFFF]/u;
const keywordRelationalOperator = /in(?:stanceof)?/y;
function babel7CompatTokens(tokens, input) {
  for (let i = 0; i < tokens.length; i++) {
    const token = tokens[i];
    const {
      type
    } = token;
    if (typeof type === "number") {
      {
        if (type === 136) {
          const {
            loc,
            start,
            value,
            end
          } = token;
          const hashEndPos = start + 1;
          const hashEndLoc = (0, _location.createPositionWithColumnOffset)(loc.start, 1);
          tokens.splice(i, 1, new _tokenizer.Token({
            type: (0, _types.getExportedToken)(27),
            value: "#",
            start: start,
            end: hashEndPos,
            startLoc: loc.start,
            endLoc: hashEndLoc
          }), new _tokenizer.Token({
            type: (0, _types.getExportedToken)(130),
            value: value,
            start: hashEndPos,
            end: end,
            startLoc: hashEndLoc,
            endLoc: loc.end
          }));
          i++;
          continue;
        }
        if ((0, _types.tokenIsTemplate)(type)) {
          const {
            loc,
            start,
            value,
            end
          } = token;
          const backquoteEnd = start + 1;
          const backquoteEndLoc = (0, _location.createPositionWithColumnOffset)(loc.start, 1);
          let startToken;
          if (input.charCodeAt(start) === 96) {
            startToken = new _tokenizer.Token({
              type: (0, _types.getExportedToken)(22),
              value: "`",
              start: start,
              end: backquoteEnd,
              startLoc: loc.start,
              endLoc: backquoteEndLoc
            });
          } else {
            startToken = new _tokenizer.Token({
              type: (0, _types.getExportedToken)(8),
              value: "}",
              start: start,
              end: backquoteEnd,
              startLoc: loc.start,
              endLoc: backquoteEndLoc
            });
          }
          let templateValue, templateElementEnd, templateElementEndLoc, endToken;
          if (type === 24) {
            templateElementEnd = end - 1;
            templateElementEndLoc = (0, _location.createPositionWithColumnOffset)(loc.end, -1);
            templateValue = value === null ? null : value.slice(1, -1);
            endToken = new _tokenizer.Token({
              type: (0, _types.getExportedToken)(22),
              value: "`",
              start: templateElementEnd,
              end: end,
              startLoc: templateElementEndLoc,
              endLoc: loc.end
            });
          } else {
            templateElementEnd = end - 2;
            templateElementEndLoc = (0, _location.createPositionWithColumnOffset)(loc.end, -2);
            templateValue = value === null ? null : value.slice(1, -2);
            endToken = new _tokenizer.Token({
              type: (0, _types.getExportedToken)(23),
              value: "${",
              start: templateElementEnd,
              end: end,
              startLoc: templateElementEndLoc,
              endLoc: loc.end
            });
          }
          tokens.splice(i, 1, startToken, new _tokenizer.Token({
            type: (0, _types.getExportedToken)(20),
            value: templateValue,
            start: backquoteEnd,
            end: templateElementEnd,
            startLoc: backquoteEndLoc,
            endLoc: templateElementEndLoc
          }), endToken);
          i += 2;
          continue;
        }
      }
      token.type = (0, _types.getExportedToken)(type);
    }
  }
  return tokens;
}
class StatementParser extends _expression.default {
  parseTopLevel(file, program) {
    file.program = this.parseProgram(program);
    file.comments = this.state.comments;
    if (this.options.tokens) {
      file.tokens = babel7CompatTokens(this.tokens, this.input);
    }
    return this.finishNode(file, "File");
  }
  parseProgram(program, end = 137, sourceType = this.options.sourceType) {
    program.sourceType = sourceType;
    program.interpreter = this.parseInterpreterDirective();
    this.parseBlockBody(program, true, true, end);
    if (this.inModule && !this.options.allowUndeclaredExports && this.scope.undefinedExports.size > 0) {
      for (const [localName, at] of Array.from(this.scope.undefinedExports)) {
        this.raise(_parseError.Errors.ModuleExportUndefined, {
          at,
          localName
        });
      }
    }
    let finishedProgram;
    if (end === 137) {
      finishedProgram = this.finishNode(program, "Program");
    } else {
      finishedProgram = this.finishNodeAt(program, "Program", (0, _location.createPositionWithColumnOffset)(this.state.startLoc, -1));
    }
    return finishedProgram;
  }
  stmtToDirective(stmt) {
    const directive = stmt;
    directive.type = "Directive";
    directive.value = directive.expression;
    delete directive.expression;
    const directiveLiteral = directive.value;
    const expressionValue = directiveLiteral.value;
    const raw = this.input.slice(directiveLiteral.start, directiveLiteral.end);
    const val = directiveLiteral.value = raw.slice(1, -1);
    this.addExtra(directiveLiteral, "raw", raw);
    this.addExtra(directiveLiteral, "rawValue", val);
    this.addExtra(directiveLiteral, "expressionValue", expressionValue);
    directiveLiteral.type = "DirectiveLiteral";
    return directive;
  }
  parseInterpreterDirective() {
    if (!this.match(28)) {
      return null;
    }
    const node = this.startNode();
    node.value = this.state.value;
    this.next();
    return this.finishNode(node, "InterpreterDirective");
  }
  isLet() {
    if (!this.isContextual(99)) {
      return false;
    }
    return this.hasFollowingBindingAtom();
  }
  chStartsBindingIdentifier(ch, pos) {
    if ((0, _identifier.isIdentifierStart)(ch)) {
      keywordRelationalOperator.lastIndex = pos;
      if (keywordRelationalOperator.test(this.input)) {
        const endCh = this.codePointAtPos(keywordRelationalOperator.lastIndex);
        if (!(0, _identifier.isIdentifierChar)(endCh) && endCh !== 92) {
          return false;
        }
      }
      return true;
    } else if (ch === 92) {
      return true;
    } else {
      return false;
    }
  }
  chStartsBindingPattern(ch) {
    return ch === 91 || ch === 123;
  }
  hasFollowingBindingAtom() {
    const next = this.nextTokenStart();
    const nextCh = this.codePointAtPos(next);
    return this.chStartsBindingPattern(nextCh) || this.chStartsBindingIdentifier(nextCh, next);
  }
  hasInLineFollowingBindingIdentifier() {
    const next = this.nextTokenInLineStart();
    const nextCh = this.codePointAtPos(next);
    return this.chStartsBindingIdentifier(nextCh, next);
  }
  startsUsingForOf() {
    const {
      type,
      containsEsc
    } = this.lookahead();
    if (type === 101 && !containsEsc) {
      return false;
    } else if ((0, _types.tokenIsIdentifier)(type) && !this.hasFollowingLineBreak()) {
      this.expectPlugin("explicitResourceManagement");
      return true;
    }
  }
  startsAwaitUsing() {
    let next = this.nextTokenInLineStart();
    if (this.isUnparsedContextual(next, "using")) {
      next = this.nextTokenInLineStartSince(next + 5);
      const nextCh = this.codePointAtPos(next);
      if (this.chStartsBindingIdentifier(nextCh, next)) {
        this.expectPlugin("explicitResourceManagement");
        return true;
      }
    }
    return false;
  }
  parseModuleItem() {
    return this.parseStatementLike(ParseStatementFlag.AllowImportExport | ParseStatementFlag.AllowDeclaration | ParseStatementFlag.AllowFunctionDeclaration | ParseStatementFlag.AllowLabeledFunction);
  }
  parseStatementListItem() {
    return this.parseStatementLike(ParseStatementFlag.AllowDeclaration | ParseStatementFlag.AllowFunctionDeclaration | (!this.options.annexB || this.state.strict ? 0 : ParseStatementFlag.AllowLabeledFunction));
  }
  parseStatementOrSloppyAnnexBFunctionDeclaration(allowLabeledFunction = false) {
    let flags = ParseStatementFlag.StatementOnly;
    if (this.options.annexB && !this.state.strict) {
      flags |= ParseStatementFlag.AllowFunctionDeclaration;
      if (allowLabeledFunction) {
        flags |= ParseStatementFlag.AllowLabeledFunction;
      }
    }
    return this.parseStatementLike(flags);
  }
  parseStatement() {
    return this.parseStatementLike(ParseStatementFlag.StatementOnly);
  }
  parseStatementLike(flags) {
    let decorators = null;
    if (this.match(26)) {
      decorators = this.parseDecorators(true);
    }
    return this.parseStatementContent(flags, decorators);
  }
  parseStatementContent(flags, decorators) {
    const starttype = this.state.type;
    const node = this.startNode();
    const allowDeclaration = !!(flags & ParseStatementFlag.AllowDeclaration);
    const allowFunctionDeclaration = !!(flags & ParseStatementFlag.AllowFunctionDeclaration);
    const topLevel = flags & ParseStatementFlag.AllowImportExport;
    switch (starttype) {
      case 60:
        return this.parseBreakContinueStatement(node, true);
      case 63:
        return this.parseBreakContinueStatement(node, false);
      case 64:
        return this.parseDebuggerStatement(node);
      case 90:
        return this.parseDoWhileStatement(node);
      case 91:
        return this.parseForStatement(node);
      case 68:
        if (this.lookaheadCharCode() === 46) break;
        if (!allowFunctionDeclaration) {
          this.raise(this.state.strict ? _parseError.Errors.StrictFunction : this.options.annexB ? _parseError.Errors.SloppyFunctionAnnexB : _parseError.Errors.SloppyFunction, {
            at: this.state.startLoc
          });
        }
        return this.parseFunctionStatement(node, false, !allowDeclaration && allowFunctionDeclaration);
      case 80:
        if (!allowDeclaration) this.unexpected();
        return this.parseClass(this.maybeTakeDecorators(decorators, node), true);
      case 69:
        return this.parseIfStatement(node);
      case 70:
        return this.parseReturnStatement(node);
      case 71:
        return this.parseSwitchStatement(node);
      case 72:
        return this.parseThrowStatement(node);
      case 73:
        return this.parseTryStatement(node);
      case 96:
        if (!this.state.containsEsc && this.startsAwaitUsing()) {
          if (!this.isAwaitAllowed()) {
            this.raise(_parseError.Errors.AwaitUsingNotInAsyncContext, {
              at: node
            });
          } else if (!allowDeclaration) {
            this.raise(_parseError.Errors.UnexpectedLexicalDeclaration, {
              at: node
            });
          }
          this.next();
          return this.parseVarStatement(node, "await using");
        }
        break;
      case 105:
        if (this.state.containsEsc || !this.hasInLineFollowingBindingIdentifier()) {
          break;
        }
        this.expectPlugin("explicitResourceManagement");
        if (!this.scope.inModule && this.scope.inTopLevel) {
          this.raise(_parseError.Errors.UnexpectedUsingDeclaration, {
            at: this.state.startLoc
          });
        } else if (!allowDeclaration) {
          this.raise(_parseError.Errors.UnexpectedLexicalDeclaration, {
            at: this.state.startLoc
          });
        }
        return this.parseVarStatement(node, "using");
      case 99:
        {
          if (this.state.containsEsc) {
            break;
          }
          const next = this.nextTokenStart();
          const nextCh = this.codePointAtPos(next);
          if (nextCh !== 91) {
            if (!allowDeclaration && this.hasFollowingLineBreak()) break;
            if (!this.chStartsBindingIdentifier(nextCh, next) && nextCh !== 123) {
              break;
            }
          }
        }
      case 75:
        {
          if (!allowDeclaration) {
            this.raise(_parseError.Errors.UnexpectedLexicalDeclaration, {
              at: this.state.startLoc
            });
          }
        }
      case 74:
        {
          const kind = this.state.value;
          return this.parseVarStatement(node, kind);
        }
      case 92:
        return this.parseWhileStatement(node);
      case 76:
        return this.parseWithStatement(node);
      case 5:
        return this.parseBlock();
      case 13:
        return this.parseEmptyStatement(node);
      case 83:
        {
          const nextTokenCharCode = this.lookaheadCharCode();
          if (nextTokenCharCode === 40 || nextTokenCharCode === 46) {
            break;
          }
        }
      case 82:
        {
          if (!this.options.allowImportExportEverywhere && !topLevel) {
            this.raise(_parseError.Errors.UnexpectedImportExport, {
              at: this.state.startLoc
            });
          }
          this.next();
          let result;
          if (starttype === 83) {
            result = this.parseImport(node);
            if (result.type === "ImportDeclaration" && (!result.importKind || result.importKind === "value")) {
              this.sawUnambiguousESM = true;
            }
          } else {
            result = this.parseExport(node, decorators);
            if (result.type === "ExportNamedDeclaration" && (!result.exportKind || result.exportKind === "value") || result.type === "ExportAllDeclaration" && (!result.exportKind || result.exportKind === "value") || result.type === "ExportDefaultDeclaration") {
              this.sawUnambiguousESM = true;
            }
          }
          this.assertModuleNodeAllowed(result);
          return result;
        }
      default:
        {
          if (this.isAsyncFunction()) {
            if (!allowDeclaration) {
              this.raise(_parseError.Errors.AsyncFunctionInSingleStatementContext, {
                at: this.state.startLoc
              });
            }
            this.next();
            return this.parseFunctionStatement(node, true, !allowDeclaration && allowFunctionDeclaration);
          }
        }
    }
    const maybeName = this.state.value;
    const expr = this.parseExpression();
    if ((0, _types.tokenIsIdentifier)(starttype) && expr.type === "Identifier" && this.eat(14)) {
      return this.parseLabeledStatement(node, maybeName, expr, flags);
    } else {
      return this.parseExpressionStatement(node, expr, decorators);
    }
  }
  assertModuleNodeAllowed(node) {
    if (!this.options.allowImportExportEverywhere && !this.inModule) {
      this.raise(_parseError.Errors.ImportOutsideModule, {
        at: node
      });
    }
  }
  decoratorsEnabledBeforeExport() {
    if (this.hasPlugin("decorators-legacy")) return true;
    return this.hasPlugin("decorators") && this.getPluginOption("decorators", "decoratorsBeforeExport") !== false;
  }
  maybeTakeDecorators(maybeDecorators, classNode, exportNode) {
    if (maybeDecorators) {
      if (classNode.decorators && classNode.decorators.length > 0) {
        if (typeof this.getPluginOption("decorators", "decoratorsBeforeExport") !== "boolean") {
          this.raise(_parseError.Errors.DecoratorsBeforeAfterExport, {
            at: classNode.decorators[0]
          });
        }
        classNode.decorators.unshift(...maybeDecorators);
      } else {
        classNode.decorators = maybeDecorators;
      }
      this.resetStartLocationFromNode(classNode, maybeDecorators[0]);
      if (exportNode) this.resetStartLocationFromNode(exportNode, classNode);
    }
    return classNode;
  }
  canHaveLeadingDecorator() {
    return this.match(80);
  }
  parseDecorators(allowExport) {
    const decorators = [];
    do {
      decorators.push(this.parseDecorator());
    } while (this.match(26));
    if (this.match(82)) {
      if (!allowExport) {
        this.unexpected();
      }
      if (!this.decoratorsEnabledBeforeExport()) {
        this.raise(_parseError.Errors.DecoratorExportClass, {
          at: this.state.startLoc
        });
      }
    } else if (!this.canHaveLeadingDecorator()) {
      throw this.raise(_parseError.Errors.UnexpectedLeadingDecorator, {
        at: this.state.startLoc
      });
    }
    return decorators;
  }
  parseDecorator() {
    this.expectOnePlugin(["decorators", "decorators-legacy"]);
    const node = this.startNode();
    this.next();
    if (this.hasPlugin("decorators")) {
      const startLoc = this.state.startLoc;
      let expr;
      if (this.match(10)) {
        const startLoc = this.state.startLoc;
        this.next();
        expr = this.parseExpression();
        this.expect(11);
        expr = this.wrapParenthesis(startLoc, expr);
        const paramsStartLoc = this.state.startLoc;
        node.expression = this.parseMaybeDecoratorArguments(expr);
        if (this.getPluginOption("decorators", "allowCallParenthesized") === false && node.expression !== expr) {
          this.raise(_parseError.Errors.DecoratorArgumentsOutsideParentheses, {
            at: paramsStartLoc
          });
        }
      } else {
        expr = this.parseIdentifier(false);
        while (this.eat(16)) {
          const node = this.startNodeAt(startLoc);
          node.object = expr;
          if (this.match(136)) {
            this.classScope.usePrivateName(this.state.value, this.state.startLoc);
            node.property = this.parsePrivateName();
          } else {
            node.property = this.parseIdentifier(true);
          }
          node.computed = false;
          expr = this.finishNode(node, "MemberExpression");
        }
        node.expression = this.parseMaybeDecoratorArguments(expr);
      }
    } else {
      node.expression = this.parseExprSubscripts();
    }
    return this.finishNode(node, "Decorator");
  }
  parseMaybeDecoratorArguments(expr) {
    if (this.eat(10)) {
      const node = this.startNodeAtNode(expr);
      node.callee = expr;
      node.arguments = this.parseCallExpressionArguments(11, false);
      this.toReferencedList(node.arguments);
      return this.finishNode(node, "CallExpression");
    }
    return expr;
  }
  parseBreakContinueStatement(node, isBreak) {
    this.next();
    if (this.isLineTerminator()) {
      node.label = null;
    } else {
      node.label = this.parseIdentifier();
      this.semicolon();
    }
    this.verifyBreakContinue(node, isBreak);
    return this.finishNode(node, isBreak ? "BreakStatement" : "ContinueStatement");
  }
  verifyBreakContinue(node, isBreak) {
    let i;
    for (i = 0; i < this.state.labels.length; ++i) {
      const lab = this.state.labels[i];
      if (node.label == null || lab.name === node.label.name) {
        if (lab.kind != null && (isBreak || lab.kind === "loop")) break;
        if (node.label && isBreak) break;
      }
    }
    if (i === this.state.labels.length) {
      const type = isBreak ? "BreakStatement" : "ContinueStatement";
      this.raise(_parseError.Errors.IllegalBreakContinue, {
        at: node,
        type
      });
    }
  }
  parseDebuggerStatement(node) {
    this.next();
    this.semicolon();
    return this.finishNode(node, "DebuggerStatement");
  }
  parseHeaderExpression() {
    this.expect(10);
    const val = this.parseExpression();
    this.expect(11);
    return val;
  }
  parseDoWhileStatement(node) {
    this.next();
    this.state.labels.push(loopLabel);
    node.body = this.withSmartMixTopicForbiddingContext(() => this.parseStatement());
    this.state.labels.pop();
    this.expect(92);
    node.test = this.parseHeaderExpression();
    this.eat(13);
    return this.finishNode(node, "DoWhileStatement");
  }
  parseForStatement(node) {
    this.next();
    this.state.labels.push(loopLabel);
    let awaitAt = null;
    if (this.isAwaitAllowed() && this.eatContextual(96)) {
      awaitAt = this.state.lastTokStartLoc;
    }
    this.scope.enter(_scopeflags.SCOPE_OTHER);
    this.expect(10);
    if (this.match(13)) {
      if (awaitAt !== null) {
        this.unexpected(awaitAt);
      }
      return this.parseFor(node, null);
    }
    const startsWithLet = this.isContextual(99);
    {
      const startsWithAwaitUsing = this.isContextual(96) && this.startsAwaitUsing();
      const starsWithUsingDeclaration = startsWithAwaitUsing || this.isContextual(105) && this.startsUsingForOf();
      const isLetOrUsing = startsWithLet && this.hasFollowingBindingAtom() || starsWithUsingDeclaration;
      if (this.match(74) || this.match(75) || isLetOrUsing) {
        const initNode = this.startNode();
        let kind;
        if (startsWithAwaitUsing) {
          kind = "await using";
          if (!this.isAwaitAllowed()) {
            this.raise(_parseError.Errors.AwaitUsingNotInAsyncContext, {
              at: this.state.startLoc
            });
          }
          this.next();
        } else {
          kind = this.state.value;
        }
        this.next();
        this.parseVar(initNode, true, kind);
        const init = this.finishNode(initNode, "VariableDeclaration");
        const isForIn = this.match(58);
        if (isForIn && starsWithUsingDeclaration) {
          this.raise(_parseError.Errors.ForInUsing, {
            at: init
          });
        }
        if ((isForIn || this.isContextual(101)) && init.declarations.length === 1) {
          return this.parseForIn(node, init, awaitAt);
        }
        if (awaitAt !== null) {
          this.unexpected(awaitAt);
        }
        return this.parseFor(node, init);
      }
    }
    const startsWithAsync = this.isContextual(95);
    const refExpressionErrors = new _util.ExpressionErrors();
    const init = this.parseExpression(true, refExpressionErrors);
    const isForOf = this.isContextual(101);
    if (isForOf) {
      if (startsWithLet) {
        this.raise(_parseError.Errors.ForOfLet, {
          at: init
        });
      }
      if (awaitAt === null && startsWithAsync && init.type === "Identifier") {
        this.raise(_parseError.Errors.ForOfAsync, {
          at: init
        });
      }
    }
    if (isForOf || this.match(58)) {
      this.checkDestructuringPrivate(refExpressionErrors);
      this.toAssignable(init, true);
      const type = isForOf ? "ForOfStatement" : "ForInStatement";
      this.checkLVal(init, {
        in: {
          type
        }
      });
      return this.parseForIn(node, init, awaitAt);
    } else {
      this.checkExpressionErrors(refExpressionErrors, true);
    }
    if (awaitAt !== null) {
      this.unexpected(awaitAt);
    }
    return this.parseFor(node, init);
  }
  parseFunctionStatement(node, isAsync, isHangingDeclaration) {
    this.next();
    return this.parseFunction(node, ParseFunctionFlag.Declaration | (isHangingDeclaration ? ParseFunctionFlag.HangingDeclaration : 0) | (isAsync ? ParseFunctionFlag.Async : 0));
  }
  parseIfStatement(node) {
    this.next();
    node.test = this.parseHeaderExpression();
    node.consequent = this.parseStatementOrSloppyAnnexBFunctionDeclaration();
    node.alternate = this.eat(66) ? this.parseStatementOrSloppyAnnexBFunctionDeclaration() : null;
    return this.finishNode(node, "IfStatement");
  }
  parseReturnStatement(node) {
    if (!this.prodParam.hasReturn && !this.options.allowReturnOutsideFunction) {
      this.raise(_parseError.Errors.IllegalReturn, {
        at: this.state.startLoc
      });
    }
    this.next();
    if (this.isLineTerminator()) {
      node.argument = null;
    } else {
      node.argument = this.parseExpression();
      this.semicolon();
    }
    return this.finishNode(node, "ReturnStatement");
  }
  parseSwitchStatement(node) {
    this.next();
    node.discriminant = this.parseHeaderExpression();
    const cases = node.cases = [];
    this.expect(5);
    this.state.labels.push(switchLabel);
    this.scope.enter(_scopeflags.SCOPE_OTHER);
    let cur;
    for (let sawDefault; !this.match(8);) {
      if (this.match(61) || this.match(65)) {
        const isCase = this.match(61);
        if (cur) this.finishNode(cur, "SwitchCase");
        cases.push(cur = this.startNode());
        cur.consequent = [];
        this.next();
        if (isCase) {
          cur.test = this.parseExpression();
        } else {
          if (sawDefault) {
            this.raise(_parseError.Errors.MultipleDefaultsInSwitch, {
              at: this.state.lastTokStartLoc
            });
          }
          sawDefault = true;
          cur.test = null;
        }
        this.expect(14);
      } else {
        if (cur) {
          cur.consequent.push(this.parseStatementListItem());
        } else {
          this.unexpected();
        }
      }
    }
    this.scope.exit();
    if (cur) this.finishNode(cur, "SwitchCase");
    this.next();
    this.state.labels.pop();
    return this.finishNode(node, "SwitchStatement");
  }
  parseThrowStatement(node) {
    this.next();
    if (this.hasPrecedingLineBreak()) {
      this.raise(_parseError.Errors.NewlineAfterThrow, {
        at: this.state.lastTokEndLoc
      });
    }
    node.argument = this.parseExpression();
    this.semicolon();
    return this.finishNode(node, "ThrowStatement");
  }
  parseCatchClauseParam() {
    const param = this.parseBindingAtom();
    this.scope.enter(this.options.annexB && param.type === "Identifier" ? _scopeflags.SCOPE_SIMPLE_CATCH : 0);
    this.checkLVal(param, {
      in: {
        type: "CatchClause"
      },
      binding: _scopeflags.BIND_CATCH_PARAM
    });
    return param;
  }
  parseTryStatement(node) {
    this.next();
    node.block = this.parseBlock();
    node.handler = null;
    if (this.match(62)) {
      const clause = this.startNode();
      this.next();
      if (this.match(10)) {
        this.expect(10);
        clause.param = this.parseCatchClauseParam();
        this.expect(11);
      } else {
        clause.param = null;
        this.scope.enter(_scopeflags.SCOPE_OTHER);
      }
      clause.body = this.withSmartMixTopicForbiddingContext(() => this.parseBlock(false, false));
      this.scope.exit();
      node.handler = this.finishNode(clause, "CatchClause");
    }
    node.finalizer = this.eat(67) ? this.parseBlock() : null;
    if (!node.handler && !node.finalizer) {
      this.raise(_parseError.Errors.NoCatchOrFinally, {
        at: node
      });
    }
    return this.finishNode(node, "TryStatement");
  }
  parseVarStatement(node, kind, allowMissingInitializer = false) {
    this.next();
    this.parseVar(node, false, kind, allowMissingInitializer);
    this.semicolon();
    return this.finishNode(node, "VariableDeclaration");
  }
  parseWhileStatement(node) {
    this.next();
    node.test = this.parseHeaderExpression();
    this.state.labels.push(loopLabel);
    node.body = this.withSmartMixTopicForbiddingContext(() => this.parseStatement());
    this.state.labels.pop();
    return this.finishNode(node, "WhileStatement");
  }
  parseWithStatement(node) {
    if (this.state.strict) {
      this.raise(_parseError.Errors.StrictWith, {
        at: this.state.startLoc
      });
    }
    this.next();
    node.object = this.parseHeaderExpression();
    node.body = this.withSmartMixTopicForbiddingContext(() => this.parseStatement());
    return this.finishNode(node, "WithStatement");
  }
  parseEmptyStatement(node) {
    this.next();
    return this.finishNode(node, "EmptyStatement");
  }
  parseLabeledStatement(node, maybeName, expr, flags) {
    for (const label of this.state.labels) {
      if (label.name === maybeName) {
        this.raise(_parseError.Errors.LabelRedeclaration, {
          at: expr,
          labelName: maybeName
        });
      }
    }
    const kind = (0, _types.tokenIsLoop)(this.state.type) ? "loop" : this.match(71) ? "switch" : null;
    for (let i = this.state.labels.length - 1; i >= 0; i--) {
      const label = this.state.labels[i];
      if (label.statementStart === node.start) {
        label.statementStart = this.state.start;
        label.kind = kind;
      } else {
        break;
      }
    }
    this.state.labels.push({
      name: maybeName,
      kind: kind,
      statementStart: this.state.start
    });
    node.body = flags & ParseStatementFlag.AllowLabeledFunction ? this.parseStatementOrSloppyAnnexBFunctionDeclaration(true) : this.parseStatement();
    this.state.labels.pop();
    node.label = expr;
    return this.finishNode(node, "LabeledStatement");
  }
  parseExpressionStatement(node, expr, decorators) {
    node.expression = expr;
    this.semicolon();
    return this.finishNode(node, "ExpressionStatement");
  }
  parseBlock(allowDirectives = false, createNewLexicalScope = true, afterBlockParse) {
    const node = this.startNode();
    if (allowDirectives) {
      this.state.strictErrors.clear();
    }
    this.expect(5);
    if (createNewLexicalScope) {
      this.scope.enter(_scopeflags.SCOPE_OTHER);
    }
    this.parseBlockBody(node, allowDirectives, false, 8, afterBlockParse);
    if (createNewLexicalScope) {
      this.scope.exit();
    }
    return this.finishNode(node, "BlockStatement");
  }
  isValidDirective(stmt) {
    return stmt.type === "ExpressionStatement" && stmt.expression.type === "StringLiteral" && !stmt.expression.extra.parenthesized;
  }
  parseBlockBody(node, allowDirectives, topLevel, end, afterBlockParse) {
    const body = node.body = [];
    const directives = node.directives = [];
    this.parseBlockOrModuleBlockBody(body, allowDirectives ? directives : undefined, topLevel, end, afterBlockParse);
  }
  parseBlockOrModuleBlockBody(body, directives, topLevel, end, afterBlockParse) {
    const oldStrict = this.state.strict;
    let hasStrictModeDirective = false;
    let parsedNonDirective = false;
    while (!this.match(end)) {
      const stmt = topLevel ? this.parseModuleItem() : this.parseStatementListItem();
      if (directives && !parsedNonDirective) {
        if (this.isValidDirective(stmt)) {
          const directive = this.stmtToDirective(stmt);
          directives.push(directive);
          if (!hasStrictModeDirective && directive.value.value === "use strict") {
            hasStrictModeDirective = true;
            this.setStrict(true);
          }
          continue;
        }
        parsedNonDirective = true;
        this.state.strictErrors.clear();
      }
      body.push(stmt);
    }
    afterBlockParse == null ? void 0 : afterBlockParse.call(this, hasStrictModeDirective);
    if (!oldStrict) {
      this.setStrict(false);
    }
    this.next();
  }
  parseFor(node, init) {
    node.init = init;
    this.semicolon(false);
    node.test = this.match(13) ? null : this.parseExpression();
    this.semicolon(false);
    node.update = this.match(11) ? null : this.parseExpression();
    this.expect(11);
    node.body = this.withSmartMixTopicForbiddingContext(() => this.parseStatement());
    this.scope.exit();
    this.state.labels.pop();
    return this.finishNode(node, "ForStatement");
  }
  parseForIn(node, init, awaitAt) {
    const isForIn = this.match(58);
    this.next();
    if (isForIn) {
      if (awaitAt !== null) this.unexpected(awaitAt);
    } else {
      node.await = awaitAt !== null;
    }
    if (init.type === "VariableDeclaration" && init.declarations[0].init != null && (!isForIn || !this.options.annexB || this.state.strict || init.kind !== "var" || init.declarations[0].id.type !== "Identifier")) {
      this.raise(_parseError.Errors.ForInOfLoopInitializer, {
        at: init,
        type: isForIn ? "ForInStatement" : "ForOfStatement"
      });
    }
    if (init.type === "AssignmentPattern") {
      this.raise(_parseError.Errors.InvalidLhs, {
        at: init,
        ancestor: {
          type: "ForStatement"
        }
      });
    }
    node.left = init;
    node.right = isForIn ? this.parseExpression() : this.parseMaybeAssignAllowIn();
    this.expect(11);
    node.body = this.withSmartMixTopicForbiddingContext(() => this.parseStatement());
    this.scope.exit();
    this.state.labels.pop();
    return this.finishNode(node, isForIn ? "ForInStatement" : "ForOfStatement");
  }
  parseVar(node, isFor, kind, allowMissingInitializer = false) {
    const declarations = node.declarations = [];
    node.kind = kind;
    for (;;) {
      const decl = this.startNode();
      this.parseVarId(decl, kind);
      decl.init = !this.eat(29) ? null : isFor ? this.parseMaybeAssignDisallowIn() : this.parseMaybeAssignAllowIn();
      if (decl.init === null && !allowMissingInitializer) {
        if (decl.id.type !== "Identifier" && !(isFor && (this.match(58) || this.isContextual(101)))) {
          this.raise(_parseError.Errors.DeclarationMissingInitializer, {
            at: this.state.lastTokEndLoc,
            kind: "destructuring"
          });
        } else if (kind === "const" && !(this.match(58) || this.isContextual(101))) {
          this.raise(_parseError.Errors.DeclarationMissingInitializer, {
            at: this.state.lastTokEndLoc,
            kind: "const"
          });
        }
      }
      declarations.push(this.finishNode(decl, "VariableDeclarator"));
      if (!this.eat(12)) break;
    }
    return node;
  }
  parseVarId(decl, kind) {
    const id = this.parseBindingAtom();
    this.checkLVal(id, {
      in: {
        type: "VariableDeclarator"
      },
      binding: kind === "var" ? _scopeflags.BIND_VAR : _scopeflags.BIND_LEXICAL
    });
    decl.id = id;
  }
  parseAsyncFunctionExpression(node) {
    return this.parseFunction(node, ParseFunctionFlag.Async);
  }
  parseFunction(node, flags = ParseFunctionFlag.Expression) {
    const hangingDeclaration = flags & ParseFunctionFlag.HangingDeclaration;
    const isDeclaration = !!(flags & ParseFunctionFlag.Declaration);
    const requireId = isDeclaration && !(flags & ParseFunctionFlag.NullableId);
    const isAsync = !!(flags & ParseFunctionFlag.Async);
    this.initFunction(node, isAsync);
    if (this.match(55)) {
      if (hangingDeclaration) {
        this.raise(_parseError.Errors.GeneratorInSingleStatementContext, {
          at: this.state.startLoc
        });
      }
      this.next();
      node.generator = true;
    }
    if (isDeclaration) {
      node.id = this.parseFunctionId(requireId);
    }
    const oldMaybeInArrowParameters = this.state.maybeInArrowParameters;
    this.state.maybeInArrowParameters = false;
    this.scope.enter(_scopeflags.SCOPE_FUNCTION);
    this.prodParam.enter((0, _productionParameter.functionFlags)(isAsync, node.generator));
    if (!isDeclaration) {
      node.id = this.parseFunctionId();
    }
    this.parseFunctionParams(node, false);
    this.withSmartMixTopicForbiddingContext(() => {
      this.parseFunctionBodyAndFinish(node, isDeclaration ? "FunctionDeclaration" : "FunctionExpression");
    });
    this.prodParam.exit();
    this.scope.exit();
    if (isDeclaration && !hangingDeclaration) {
      this.registerFunctionStatementId(node);
    }
    this.state.maybeInArrowParameters = oldMaybeInArrowParameters;
    return node;
  }
  parseFunctionId(requireId) {
    return requireId || (0, _types.tokenIsIdentifier)(this.state.type) ? this.parseIdentifier() : null;
  }
  parseFunctionParams(node, isConstructor) {
    this.expect(10);
    this.expressionScope.enter((0, _expressionScope.newParameterDeclarationScope)());
    node.params = this.parseBindingList(11, 41, _lval.ParseBindingListFlags.IS_FUNCTION_PARAMS | (isConstructor ? _lval.ParseBindingListFlags.IS_CONSTRUCTOR_PARAMS : 0));
    this.expressionScope.exit();
  }
  registerFunctionStatementId(node) {
    if (!node.id) return;
    this.scope.declareName(node.id.name, !this.options.annexB || this.state.strict || node.generator || node.async ? this.scope.treatFunctionsAsVar ? _scopeflags.BIND_VAR : _scopeflags.BIND_LEXICAL : _scopeflags.BIND_FUNCTION, node.id.loc.start);
  }
  parseClass(node, isStatement, optionalId) {
    this.next();
    const oldStrict = this.state.strict;
    this.state.strict = true;
    this.parseClassId(node, isStatement, optionalId);
    this.parseClassSuper(node);
    node.body = this.parseClassBody(!!node.superClass, oldStrict);
    return this.finishNode(node, isStatement ? "ClassDeclaration" : "ClassExpression");
  }
  isClassProperty() {
    return this.match(29) || this.match(13) || this.match(8);
  }
  isClassMethod() {
    return this.match(10);
  }
  isNonstaticConstructor(method) {
    return !method.computed && !method.static && (method.key.name === "constructor" || method.key.value === "constructor");
  }
  parseClassBody(hadSuperClass, oldStrict) {
    this.classScope.enter();
    const state = {
      hadConstructor: false,
      hadSuperClass
    };
    let decorators = [];
    const classBody = this.startNode();
    classBody.body = [];
    this.expect(5);
    this.withSmartMixTopicForbiddingContext(() => {
      while (!this.match(8)) {
        if (this.eat(13)) {
          if (decorators.length > 0) {
            throw this.raise(_parseError.Errors.DecoratorSemicolon, {
              at: this.state.lastTokEndLoc
            });
          }
          continue;
        }
        if (this.match(26)) {
          decorators.push(this.parseDecorator());
          continue;
        }
        const member = this.startNode();
        if (decorators.length) {
          member.decorators = decorators;
          this.resetStartLocationFromNode(member, decorators[0]);
          decorators = [];
        }
        this.parseClassMember(classBody, member, state);
        if (member.kind === "constructor" && member.decorators && member.decorators.length > 0) {
          this.raise(_parseError.Errors.DecoratorConstructor, {
            at: member
          });
        }
      }
    });
    this.state.strict = oldStrict;
    this.next();
    if (decorators.length) {
      throw this.raise(_parseError.Errors.TrailingDecorator, {
        at: this.state.startLoc
      });
    }
    this.classScope.exit();
    return this.finishNode(classBody, "ClassBody");
  }
  parseClassMemberFromModifier(classBody, member) {
    const key = this.parseIdentifier(true);
    if (this.isClassMethod()) {
      const method = member;
      method.kind = "method";
      method.computed = false;
      method.key = key;
      method.static = false;
      this.pushClassMethod(classBody, method, false, false, false, false);
      return true;
    } else if (this.isClassProperty()) {
      const prop = member;
      prop.computed = false;
      prop.key = key;
      prop.static = false;
      classBody.body.push(this.parseClassProperty(prop));
      return true;
    }
    this.resetPreviousNodeTrailingComments(key);
    return false;
  }
  parseClassMember(classBody, member, state) {
    const isStatic = this.isContextual(104);
    if (isStatic) {
      if (this.parseClassMemberFromModifier(classBody, member)) {
        return;
      }
      if (this.eat(5)) {
        this.parseClassStaticBlock(classBody, member);
        return;
      }
    }
    this.parseClassMemberWithIsStatic(classBody, member, state, isStatic);
  }
  parseClassMemberWithIsStatic(classBody, member, state, isStatic) {
    const publicMethod = member;
    const privateMethod = member;
    const publicProp = member;
    const privateProp = member;
    const accessorProp = member;
    const method = publicMethod;
    const publicMember = publicMethod;
    member.static = isStatic;
    this.parsePropertyNamePrefixOperator(member);
    if (this.eat(55)) {
      method.kind = "method";
      const isPrivateName = this.match(136);
      this.parseClassElementName(method);
      if (isPrivateName) {
        this.pushClassPrivateMethod(classBody, privateMethod, true, false);
        return;
      }
      if (this.isNonstaticConstructor(publicMethod)) {
        this.raise(_parseError.Errors.ConstructorIsGenerator, {
          at: publicMethod.key
        });
      }
      this.pushClassMethod(classBody, publicMethod, true, false, false, false);
      return;
    }
    const isContextual = (0, _types.tokenIsIdentifier)(this.state.type) && !this.state.containsEsc;
    const isPrivate = this.match(136);
    const key = this.parseClassElementName(member);
    const maybeQuestionTokenStartLoc = this.state.startLoc;
    this.parsePostMemberNameModifiers(publicMember);
    if (this.isClassMethod()) {
      method.kind = "method";
      if (isPrivate) {
        this.pushClassPrivateMethod(classBody, privateMethod, false, false);
        return;
      }
      const isConstructor = this.isNonstaticConstructor(publicMethod);
      let allowsDirectSuper = false;
      if (isConstructor) {
        publicMethod.kind = "constructor";
        if (state.hadConstructor && !this.hasPlugin("typescript")) {
          this.raise(_parseError.Errors.DuplicateConstructor, {
            at: key
          });
        }
        if (isConstructor && this.hasPlugin("typescript") && member.override) {
          this.raise(_parseError.Errors.OverrideOnConstructor, {
            at: key
          });
        }
        state.hadConstructor = true;
        allowsDirectSuper = state.hadSuperClass;
      }
      this.pushClassMethod(classBody, publicMethod, false, false, isConstructor, allowsDirectSuper);
    } else if (this.isClassProperty()) {
      if (isPrivate) {
        this.pushClassPrivateProperty(classBody, privateProp);
      } else {
        this.pushClassProperty(classBody, publicProp);
      }
    } else if (isContextual && key.name === "async" && !this.isLineTerminator()) {
      this.resetPreviousNodeTrailingComments(key);
      const isGenerator = this.eat(55);
      if (publicMember.optional) {
        this.unexpected(maybeQuestionTokenStartLoc);
      }
      method.kind = "method";
      const isPrivate = this.match(136);
      this.parseClassElementName(method);
      this.parsePostMemberNameModifiers(publicMember);
      if (isPrivate) {
        this.pushClassPrivateMethod(classBody, privateMethod, isGenerator, true);
      } else {
        if (this.isNonstaticConstructor(publicMethod)) {
          this.raise(_parseError.Errors.ConstructorIsAsync, {
            at: publicMethod.key
          });
        }
        this.pushClassMethod(classBody, publicMethod, isGenerator, true, false, false);
      }
    } else if (isContextual && (key.name === "get" || key.name === "set") && !(this.match(55) && this.isLineTerminator())) {
      this.resetPreviousNodeTrailingComments(key);
      method.kind = key.name;
      const isPrivate = this.match(136);
      this.parseClassElementName(publicMethod);
      if (isPrivate) {
        this.pushClassPrivateMethod(classBody, privateMethod, false, false);
      } else {
        if (this.isNonstaticConstructor(publicMethod)) {
          this.raise(_parseError.Errors.ConstructorIsAccessor, {
            at: publicMethod.key
          });
        }
        this.pushClassMethod(classBody, publicMethod, false, false, false, false);
      }
      this.checkGetterSetterParams(publicMethod);
    } else if (isContextual && key.name === "accessor" && !this.isLineTerminator()) {
      this.expectPlugin("decoratorAutoAccessors");
      this.resetPreviousNodeTrailingComments(key);
      const isPrivate = this.match(136);
      this.parseClassElementName(publicProp);
      this.pushClassAccessorProperty(classBody, accessorProp, isPrivate);
    } else if (this.isLineTerminator()) {
      if (isPrivate) {
        this.pushClassPrivateProperty(classBody, privateProp);
      } else {
        this.pushClassProperty(classBody, publicProp);
      }
    } else {
      this.unexpected();
    }
  }
  parseClassElementName(member) {
    const {
      type,
      value
    } = this.state;
    if ((type === 130 || type === 131) && member.static && value === "prototype") {
      this.raise(_parseError.Errors.StaticPrototype, {
        at: this.state.startLoc
      });
    }
    if (type === 136) {
      if (value === "constructor") {
        this.raise(_parseError.Errors.ConstructorClassPrivateField, {
          at: this.state.startLoc
        });
      }
      const key = this.parsePrivateName();
      member.key = key;
      return key;
    }
    return this.parsePropertyName(member);
  }
  parseClassStaticBlock(classBody, member) {
    var _member$decorators;
    this.scope.enter(_scopeflags.SCOPE_CLASS | _scopeflags.SCOPE_STATIC_BLOCK | _scopeflags.SCOPE_SUPER);
    const oldLabels = this.state.labels;
    this.state.labels = [];
    this.prodParam.enter(_productionParameter.PARAM);
    const body = member.body = [];
    this.parseBlockOrModuleBlockBody(body, undefined, false, 8);
    this.prodParam.exit();
    this.scope.exit();
    this.state.labels = oldLabels;
    classBody.body.push(this.finishNode(member, "StaticBlock"));
    if ((_member$decorators = member.decorators) != null && _member$decorators.length) {
      this.raise(_parseError.Errors.DecoratorStaticBlock, {
        at: member
      });
    }
  }
  pushClassProperty(classBody, prop) {
    if (!prop.computed && (prop.key.name === "constructor" || prop.key.value === "constructor")) {
      this.raise(_parseError.Errors.ConstructorClassField, {
        at: prop.key
      });
    }
    classBody.body.push(this.parseClassProperty(prop));
  }
  pushClassPrivateProperty(classBody, prop) {
    const node = this.parseClassPrivateProperty(prop);
    classBody.body.push(node);
    this.classScope.declarePrivateName(this.getPrivateNameSV(node.key), _scopeflags.CLASS_ELEMENT_OTHER, node.key.loc.start);
  }
  pushClassAccessorProperty(classBody, prop, isPrivate) {
    if (!isPrivate && !prop.computed) {
      const key = prop.key;
      if (key.name === "constructor" || key.value === "constructor") {
        this.raise(_parseError.Errors.ConstructorClassField, {
          at: key
        });
      }
    }
    const node = this.parseClassAccessorProperty(prop);
    classBody.body.push(node);
    if (isPrivate) {
      this.classScope.declarePrivateName(this.getPrivateNameSV(node.key), _scopeflags.CLASS_ELEMENT_OTHER, node.key.loc.start);
    }
  }
  pushClassMethod(classBody, method, isGenerator, isAsync, isConstructor, allowsDirectSuper) {
    classBody.body.push(this.parseMethod(method, isGenerator, isAsync, isConstructor, allowsDirectSuper, "ClassMethod", true));
  }
  pushClassPrivateMethod(classBody, method, isGenerator, isAsync) {
    const node = this.parseMethod(method, isGenerator, isAsync, false, false, "ClassPrivateMethod", true);
    classBody.body.push(node);
    const kind = node.kind === "get" ? node.static ? _scopeflags.CLASS_ELEMENT_STATIC_GETTER : _scopeflags.CLASS_ELEMENT_INSTANCE_GETTER : node.kind === "set" ? node.static ? _scopeflags.CLASS_ELEMENT_STATIC_SETTER : _scopeflags.CLASS_ELEMENT_INSTANCE_SETTER : _scopeflags.CLASS_ELEMENT_OTHER;
    this.declareClassPrivateMethodInScope(node, kind);
  }
  declareClassPrivateMethodInScope(node, kind) {
    this.classScope.declarePrivateName(this.getPrivateNameSV(node.key), kind, node.key.loc.start);
  }
  parsePostMemberNameModifiers(methodOrProp) {}
  parseClassPrivateProperty(node) {
    this.parseInitializer(node);
    this.semicolon();
    return this.finishNode(node, "ClassPrivateProperty");
  }
  parseClassProperty(node) {
    this.parseInitializer(node);
    this.semicolon();
    return this.finishNode(node, "ClassProperty");
  }
  parseClassAccessorProperty(node) {
    this.parseInitializer(node);
    this.semicolon();
    return this.finishNode(node, "ClassAccessorProperty");
  }
  parseInitializer(node) {
    this.scope.enter(_scopeflags.SCOPE_CLASS | _scopeflags.SCOPE_SUPER);
    this.expressionScope.enter((0, _expressionScope.newExpressionScope)());
    this.prodParam.enter(_productionParameter.PARAM);
    node.value = this.eat(29) ? this.parseMaybeAssignAllowIn() : null;
    this.expressionScope.exit();
    this.prodParam.exit();
    this.scope.exit();
  }
  parseClassId(node, isStatement, optionalId, bindingType = _scopeflags.BIND_CLASS) {
    if ((0, _types.tokenIsIdentifier)(this.state.type)) {
      node.id = this.parseIdentifier();
      if (isStatement) {
        this.declareNameFromIdentifier(node.id, bindingType);
      }
    } else {
      if (optionalId || !isStatement) {
        node.id = null;
      } else {
        throw this.raise(_parseError.Errors.MissingClassName, {
          at: this.state.startLoc
        });
      }
    }
  }
  parseClassSuper(node) {
    node.superClass = this.eat(81) ? this.parseExprSubscripts() : null;
  }
  parseExport(node, decorators) {
    const maybeDefaultIdentifier = this.parseMaybeImportPhase(node, true);
    const hasDefault = this.maybeParseExportDefaultSpecifier(node, maybeDefaultIdentifier);
    const parseAfterDefault = !hasDefault || this.eat(12);
    const hasStar = parseAfterDefault && this.eatExportStar(node);
    const hasNamespace = hasStar && this.maybeParseExportNamespaceSpecifier(node);
    const parseAfterNamespace = parseAfterDefault && (!hasNamespace || this.eat(12));
    const isFromRequired = hasDefault || hasStar;
    if (hasStar && !hasNamespace) {
      if (hasDefault) this.unexpected();
      if (decorators) {
        throw this.raise(_parseError.Errors.UnsupportedDecoratorExport, {
          at: node
        });
      }
      this.parseExportFrom(node, true);
      return this.finishNode(node, "ExportAllDeclaration");
    }
    const hasSpecifiers = this.maybeParseExportNamedSpecifiers(node);
    if (hasDefault && parseAfterDefault && !hasStar && !hasSpecifiers) {
      this.unexpected(null, 5);
    }
    if (hasNamespace && parseAfterNamespace) {
      this.unexpected(null, 97);
    }
    let hasDeclaration;
    if (isFromRequired || hasSpecifiers) {
      hasDeclaration = false;
      if (decorators) {
        throw this.raise(_parseError.Errors.UnsupportedDecoratorExport, {
          at: node
        });
      }
      this.parseExportFrom(node, isFromRequired);
    } else {
      hasDeclaration = this.maybeParseExportDeclaration(node);
    }
    if (isFromRequired || hasSpecifiers || hasDeclaration) {
      var _node2$declaration;
      const node2 = node;
      this.checkExport(node2, true, false, !!node2.source);
      if (((_node2$declaration = node2.declaration) == null ? void 0 : _node2$declaration.type) === "ClassDeclaration") {
        this.maybeTakeDecorators(decorators, node2.declaration, node2);
      } else if (decorators) {
        throw this.raise(_parseError.Errors.UnsupportedDecoratorExport, {
          at: node
        });
      }
      return this.finishNode(node2, "ExportNamedDeclaration");
    }
    if (this.eat(65)) {
      const node2 = node;
      const decl = this.parseExportDefaultExpression();
      node2.declaration = decl;
      if (decl.type === "ClassDeclaration") {
        this.maybeTakeDecorators(decorators, decl, node2);
      } else if (decorators) {
        throw this.raise(_parseError.Errors.UnsupportedDecoratorExport, {
          at: node
        });
      }
      this.checkExport(node2, true, true);
      return this.finishNode(node2, "ExportDefaultDeclaration");
    }
    this.unexpected(null, 5);
  }
  eatExportStar(node) {
    return this.eat(55);
  }
  maybeParseExportDefaultSpecifier(node, maybeDefaultIdentifier) {
    if (maybeDefaultIdentifier || this.isExportDefaultSpecifier()) {
      this.expectPlugin("exportDefaultFrom", maybeDefaultIdentifier == null ? void 0 : maybeDefaultIdentifier.loc.start);
      const id = maybeDefaultIdentifier || this.parseIdentifier(true);
      const specifier = this.startNodeAtNode(id);
      specifier.exported = id;
      node.specifiers = [this.finishNode(specifier, "ExportDefaultSpecifier")];
      return true;
    }
    return false;
  }
  maybeParseExportNamespaceSpecifier(node) {
    if (this.isContextual(93)) {
      if (!node.specifiers) node.specifiers = [];
      const specifier = this.startNodeAt(this.state.lastTokStartLoc);
      this.next();
      specifier.exported = this.parseModuleExportName();
      node.specifiers.push(this.finishNode(specifier, "ExportNamespaceSpecifier"));
      return true;
    }
    return false;
  }
  maybeParseExportNamedSpecifiers(node) {
    if (this.match(5)) {
      if (!node.specifiers) node.specifiers = [];
      const isTypeExport = node.exportKind === "type";
      node.specifiers.push(...this.parseExportSpecifiers(isTypeExport));
      node.source = null;
      node.declaration = null;
      if (this.hasPlugin("importAssertions")) {
        node.assertions = [];
      }
      return true;
    }
    return false;
  }
  maybeParseExportDeclaration(node) {
    if (this.shouldParseExportDeclaration()) {
      node.specifiers = [];
      node.source = null;
      if (this.hasPlugin("importAssertions")) {
        node.assertions = [];
      }
      node.declaration = this.parseExportDeclaration(node);
      return true;
    }
    return false;
  }
  isAsyncFunction() {
    if (!this.isContextual(95)) return false;
    const next = this.nextTokenInLineStart();
    return this.isUnparsedContextual(next, "function");
  }
  parseExportDefaultExpression() {
    const expr = this.startNode();
    if (this.match(68)) {
      this.next();
      return this.parseFunction(expr, ParseFunctionFlag.Declaration | ParseFunctionFlag.NullableId);
    } else if (this.isAsyncFunction()) {
      this.next();
      this.next();
      return this.parseFunction(expr, ParseFunctionFlag.Declaration | ParseFunctionFlag.NullableId | ParseFunctionFlag.Async);
    }
    if (this.match(80)) {
      return this.parseClass(expr, true, true);
    }
    if (this.match(26)) {
      if (this.hasPlugin("decorators") && this.getPluginOption("decorators", "decoratorsBeforeExport") === true) {
        this.raise(_parseError.Errors.DecoratorBeforeExport, {
          at: this.state.startLoc
        });
      }
      return this.parseClass(this.maybeTakeDecorators(this.parseDecorators(false), this.startNode()), true, true);
    }
    if (this.match(75) || this.match(74) || this.isLet()) {
      throw this.raise(_parseError.Errors.UnsupportedDefaultExport, {
        at: this.state.startLoc
      });
    }
    const res = this.parseMaybeAssignAllowIn();
    this.semicolon();
    return res;
  }
  parseExportDeclaration(node) {
    if (this.match(80)) {
      const node = this.parseClass(this.startNode(), true, false);
      return node;
    }
    return this.parseStatementListItem();
  }
  isExportDefaultSpecifier() {
    const {
      type
    } = this.state;
    if ((0, _types.tokenIsIdentifier)(type)) {
      if (type === 95 && !this.state.containsEsc || type === 99) {
        return false;
      }
      if ((type === 128 || type === 127) && !this.state.containsEsc) {
        const {
          type: nextType
        } = this.lookahead();
        if ((0, _types.tokenIsIdentifier)(nextType) && nextType !== 97 || nextType === 5) {
          this.expectOnePlugin(["flow", "typescript"]);
          return false;
        }
      }
    } else if (!this.match(65)) {
      return false;
    }
    const next = this.nextTokenStart();
    const hasFrom = this.isUnparsedContextual(next, "from");
    if (this.input.charCodeAt(next) === 44 || (0, _types.tokenIsIdentifier)(this.state.type) && hasFrom) {
      return true;
    }
    if (this.match(65) && hasFrom) {
      const nextAfterFrom = this.input.charCodeAt(this.nextTokenStartSince(next + 4));
      return nextAfterFrom === 34 || nextAfterFrom === 39;
    }
    return false;
  }
  parseExportFrom(node, expect) {
    if (this.eatContextual(97)) {
      node.source = this.parseImportSource();
      this.checkExport(node);
      this.maybeParseImportAttributes(node);
      this.checkJSONModuleImport(node);
    } else if (expect) {
      this.unexpected();
    }
    this.semicolon();
  }
  shouldParseExportDeclaration() {
    const {
      type
    } = this.state;
    if (type === 26) {
      this.expectOnePlugin(["decorators", "decorators-legacy"]);
      if (this.hasPlugin("decorators")) {
        if (this.getPluginOption("decorators", "decoratorsBeforeExport") === true) {
          this.raise(_parseError.Errors.DecoratorBeforeExport, {
            at: this.state.startLoc
          });
        }
        return true;
      }
    }
    return type === 74 || type === 75 || type === 68 || type === 80 || this.isLet() || this.isAsyncFunction();
  }
  checkExport(node, checkNames, isDefault, isFrom) {
    if (checkNames) {
      if (isDefault) {
        this.checkDuplicateExports(node, "default");
        if (this.hasPlugin("exportDefaultFrom")) {
          var _declaration$extra;
          const declaration = node.declaration;
          if (declaration.type === "Identifier" && declaration.name === "from" && declaration.end - declaration.start === 4 && !((_declaration$extra = declaration.extra) != null && _declaration$extra.parenthesized)) {
            this.raise(_parseError.Errors.ExportDefaultFromAsIdentifier, {
              at: declaration
            });
          }
        }
      } else if (node.specifiers && node.specifiers.length) {
        for (const specifier of node.specifiers) {
          const {
            exported
          } = specifier;
          const exportName = exported.type === "Identifier" ? exported.name : exported.value;
          this.checkDuplicateExports(specifier, exportName);
          if (!isFrom && specifier.local) {
            const {
              local
            } = specifier;
            if (local.type !== "Identifier") {
              this.raise(_parseError.Errors.ExportBindingIsString, {
                at: specifier,
                localName: local.value,
                exportName
              });
            } else {
              this.checkReservedWord(local.name, local.loc.start, true, false);
              this.scope.checkLocalExport(local);
            }
          }
        }
      } else if (node.declaration) {
        if (node.declaration.type === "FunctionDeclaration" || node.declaration.type === "ClassDeclaration") {
          const id = node.declaration.id;
          if (!id) throw new Error("Assertion failure");
          this.checkDuplicateExports(node, id.name);
        } else if (node.declaration.type === "VariableDeclaration") {
          for (const declaration of node.declaration.declarations) {
            this.checkDeclaration(declaration.id);
          }
        }
      }
    }
  }
  checkDeclaration(node) {
    if (node.type === "Identifier") {
      this.checkDuplicateExports(node, node.name);
    } else if (node.type === "ObjectPattern") {
      for (const prop of node.properties) {
        this.checkDeclaration(prop);
      }
    } else if (node.type === "ArrayPattern") {
      for (const elem of node.elements) {
        if (elem) {
          this.checkDeclaration(elem);
        }
      }
    } else if (node.type === "ObjectProperty") {
      this.checkDeclaration(node.value);
    } else if (node.type === "RestElement") {
      this.checkDeclaration(node.argument);
    } else if (node.type === "AssignmentPattern") {
      this.checkDeclaration(node.left);
    }
  }
  checkDuplicateExports(node, exportName) {
    if (this.exportedIdentifiers.has(exportName)) {
      if (exportName === "default") {
        this.raise(_parseError.Errors.DuplicateDefaultExport, {
          at: node
        });
      } else {
        this.raise(_parseError.Errors.DuplicateExport, {
          at: node,
          exportName
        });
      }
    }
    this.exportedIdentifiers.add(exportName);
  }
  parseExportSpecifiers(isInTypeExport) {
    const nodes = [];
    let first = true;
    this.expect(5);
    while (!this.eat(8)) {
      if (first) {
        first = false;
      } else {
        this.expect(12);
        if (this.eat(8)) break;
      }
      const isMaybeTypeOnly = this.isContextual(128);
      const isString = this.match(131);
      const node = this.startNode();
      node.local = this.parseModuleExportName();
      nodes.push(this.parseExportSpecifier(node, isString, isInTypeExport, isMaybeTypeOnly));
    }
    return nodes;
  }
  parseExportSpecifier(node, isString, isInTypeExport, isMaybeTypeOnly) {
    if (this.eatContextual(93)) {
      node.exported = this.parseModuleExportName();
    } else if (isString) {
      node.exported = (0, _node.cloneStringLiteral)(node.local);
    } else if (!node.exported) {
      node.exported = (0, _node.cloneIdentifier)(node.local);
    }
    return this.finishNode(node, "ExportSpecifier");
  }
  parseModuleExportName() {
    if (this.match(131)) {
      const result = this.parseStringLiteral(this.state.value);
      const surrogate = result.value.match(loneSurrogate);
      if (surrogate) {
        this.raise(_parseError.Errors.ModuleExportNameHasLoneSurrogate, {
          at: result,
          surrogateCharCode: surrogate[0].charCodeAt(0)
        });
      }
      return result;
    }
    return this.parseIdentifier(true);
  }
  isJSONModuleImport(node) {
    if (node.assertions != null) {
      return node.assertions.some(({
        key,
        value
      }) => {
        return value.value === "json" && (key.type === "Identifier" ? key.name === "type" : key.value === "type");
      });
    }
    return false;
  }
  checkImportReflection(node) {
    if (node.module) {
      var _node$assertions;
      if (node.specifiers.length !== 1 || node.specifiers[0].type !== "ImportDefaultSpecifier") {
        this.raise(_parseError.Errors.ImportReflectionNotBinding, {
          at: node.specifiers[0].loc.start
        });
      }
      if (((_node$assertions = node.assertions) == null ? void 0 : _node$assertions.length) > 0) {
        this.raise(_parseError.Errors.ImportReflectionHasAssertion, {
          at: node.specifiers[0].loc.start
        });
      }
    }
  }
  checkJSONModuleImport(node) {
    if (this.isJSONModuleImport(node) && node.type !== "ExportAllDeclaration") {
      const {
        specifiers
      } = node;
      if (specifiers != null) {
        const nonDefaultNamedSpecifier = specifiers.find(specifier => {
          let imported;
          if (specifier.type === "ExportSpecifier") {
            imported = specifier.local;
          } else if (specifier.type === "ImportSpecifier") {
            imported = specifier.imported;
          }
          if (imported !== undefined) {
            return imported.type === "Identifier" ? imported.name !== "default" : imported.value !== "default";
          }
        });
        if (nonDefaultNamedSpecifier !== undefined) {
          this.raise(_parseError.Errors.ImportJSONBindingNotDefault, {
            at: nonDefaultNamedSpecifier.loc.start
          });
        }
      }
    }
  }
  isPotentialImportPhase(isExport) {
    return !isExport && this.isContextual(125);
  }
  applyImportPhase(node, isExport, phase, loc) {
    if (isExport) {
      ;
      return;
    }
    if (phase === "module") {
      this.expectPlugin("importReflection", loc);
      node.module = true;
    } else if (this.hasPlugin("importReflection")) {
      node.module = false;
    }
  }
  parseMaybeImportPhase(node, isExport) {
    if (!this.isPotentialImportPhase(isExport)) {
      this.applyImportPhase(node, isExport, null);
      return null;
    }
    const phaseIdentifier = this.parseIdentifier(true);
    const {
      type
    } = this.state;
    const isImportPhase = (0, _types.tokenIsKeywordOrIdentifier)(type) ? type !== 97 || this.lookaheadCharCode() === 102 : type !== 12;
    if (isImportPhase) {
      this.resetPreviousIdentifierLeadingComments(phaseIdentifier);
      this.applyImportPhase(node, isExport, phaseIdentifier.name, phaseIdentifier.loc.start);
      return null;
    } else {
      this.applyImportPhase(node, isExport, null);
      return phaseIdentifier;
    }
  }
  isPrecedingIdImportPhase(phase) {
    const {
      type
    } = this.state;
    return (0, _types.tokenIsIdentifier)(type) ? type !== 97 || this.lookaheadCharCode() === 102 : type !== 12;
  }
  parseImport(node) {
    if (this.match(131)) {
      return this.parseImportSourceAndAttributes(node);
    }
    return this.parseImportSpecifiersAndAfter(node, this.parseMaybeImportPhase(node, false));
  }
  parseImportSpecifiersAndAfter(node, maybeDefaultIdentifier) {
    node.specifiers = [];
    const hasDefault = this.maybeParseDefaultImportSpecifier(node, maybeDefaultIdentifier);
    const parseNext = !hasDefault || this.eat(12);
    const hasStar = parseNext && this.maybeParseStarImportSpecifier(node);
    if (parseNext && !hasStar) this.parseNamedImportSpecifiers(node);
    this.expectContextual(97);
    return this.parseImportSourceAndAttributes(node);
  }
  parseImportSourceAndAttributes(node) {
    var _node$specifiers;
    (_node$specifiers = node.specifiers) != null ? _node$specifiers : node.specifiers = [];
    node.source = this.parseImportSource();
    this.maybeParseImportAttributes(node);
    this.checkImportReflection(node);
    this.checkJSONModuleImport(node);
    this.semicolon();
    return this.finishNode(node, "ImportDeclaration");
  }
  parseImportSource() {
    if (!this.match(131)) this.unexpected();
    return this.parseExprAtom();
  }
  parseImportSpecifierLocal(node, specifier, type) {
    specifier.local = this.parseIdentifier();
    node.specifiers.push(this.finishImportSpecifier(specifier, type));
  }
  finishImportSpecifier(specifier, type, bindingType = _scopeflags.BIND_LEXICAL) {
    this.checkLVal(specifier.local, {
      in: {
        type
      },
      binding: bindingType
    });
    return this.finishNode(specifier, type);
  }
  parseImportAttributes() {
    this.expect(5);
    const attrs = [];
    const attrNames = new Set();
    do {
      if (this.match(8)) {
        break;
      }
      const node = this.startNode();
      const keyName = this.state.value;
      if (attrNames.has(keyName)) {
        this.raise(_parseError.Errors.ModuleAttributesWithDuplicateKeys, {
          at: this.state.startLoc,
          key: keyName
        });
      }
      attrNames.add(keyName);
      if (this.match(131)) {
        node.key = this.parseStringLiteral(keyName);
      } else {
        node.key = this.parseIdentifier(true);
      }
      this.expect(14);
      if (!this.match(131)) {
        throw this.raise(_parseError.Errors.ModuleAttributeInvalidValue, {
          at: this.state.startLoc
        });
      }
      node.value = this.parseStringLiteral(this.state.value);
      attrs.push(this.finishNode(node, "ImportAttribute"));
    } while (this.eat(12));
    this.expect(8);
    return attrs;
  }
  parseModuleAttributes() {
    const attrs = [];
    const attributes = new Set();
    do {
      const node = this.startNode();
      node.key = this.parseIdentifier(true);
      if (node.key.name !== "type") {
        this.raise(_parseError.Errors.ModuleAttributeDifferentFromType, {
          at: node.key
        });
      }
      if (attributes.has(node.key.name)) {
        this.raise(_parseError.Errors.ModuleAttributesWithDuplicateKeys, {
          at: node.key,
          key: node.key.name
        });
      }
      attributes.add(node.key.name);
      this.expect(14);
      if (!this.match(131)) {
        throw this.raise(_parseError.Errors.ModuleAttributeInvalidValue, {
          at: this.state.startLoc
        });
      }
      node.value = this.parseStringLiteral(this.state.value);
      attrs.push(this.finishNode(node, "ImportAttribute"));
    } while (this.eat(12));
    return attrs;
  }
  maybeParseImportAttributes(node) {
    let attributes;
    let useWith = false;
    if (this.match(76)) {
      if (this.hasPrecedingLineBreak() && this.lookaheadCharCode() === 40) {
        return;
      }
      this.next();
      {
        if (this.hasPlugin("moduleAttributes")) {
          attributes = this.parseModuleAttributes();
        } else {
          this.expectImportAttributesPlugin();
          attributes = this.parseImportAttributes();
        }
      }
      useWith = true;
    } else if (this.isContextual(94) && !this.hasPrecedingLineBreak()) {
      if (this.hasPlugin("importAttributes")) {
        if (this.getPluginOption("importAttributes", "deprecatedAssertSyntax") !== true) {
          this.raise(_parseError.Errors.ImportAttributesUseAssert, {
            at: this.state.startLoc
          });
        }
        this.addExtra(node, "deprecatedAssertSyntax", true);
      } else {
        this.expectOnePlugin(["importAttributes", "importAssertions"]);
      }
      this.next();
      attributes = this.parseImportAttributes();
    } else if (this.hasPlugin("importAttributes") || this.hasPlugin("importAssertions")) {
      attributes = [];
    } else {
      if (this.hasPlugin("moduleAttributes")) {
        attributes = [];
      } else return;
    }
    if (!useWith && this.hasPlugin("importAssertions")) {
      node.assertions = attributes;
    } else {
      node.attributes = attributes;
    }
  }
  maybeParseDefaultImportSpecifier(node, maybeDefaultIdentifier) {
    if (maybeDefaultIdentifier) {
      const specifier = this.startNodeAtNode(maybeDefaultIdentifier);
      specifier.local = maybeDefaultIdentifier;
      node.specifiers.push(this.finishImportSpecifier(specifier, "ImportDefaultSpecifier"));
      return true;
    } else if ((0, _types.tokenIsKeywordOrIdentifier)(this.state.type)) {
      this.parseImportSpecifierLocal(node, this.startNode(), "ImportDefaultSpecifier");
      return true;
    }
    return false;
  }
  maybeParseStarImportSpecifier(node) {
    if (this.match(55)) {
      const specifier = this.startNode();
      this.next();
      this.expectContextual(93);
      this.parseImportSpecifierLocal(node, specifier, "ImportNamespaceSpecifier");
      return true;
    }
    return false;
  }
  parseNamedImportSpecifiers(node) {
    let first = true;
    this.expect(5);
    while (!this.eat(8)) {
      if (first) {
        first = false;
      } else {
        if (this.eat(14)) {
          throw this.raise(_parseError.Errors.DestructureNamedImport, {
            at: this.state.startLoc
          });
        }
        this.expect(12);
        if (this.eat(8)) break;
      }
      const specifier = this.startNode();
      const importedIsString = this.match(131);
      const isMaybeTypeOnly = this.isContextual(128);
      specifier.imported = this.parseModuleExportName();
      const importSpecifier = this.parseImportSpecifier(specifier, importedIsString, node.importKind === "type" || node.importKind === "typeof", isMaybeTypeOnly, undefined);
      node.specifiers.push(importSpecifier);
    }
  }
  parseImportSpecifier(specifier, importedIsString, isInTypeOnlyImport, isMaybeTypeOnly, bindingType) {
    if (this.eatContextual(93)) {
      specifier.local = this.parseIdentifier();
    } else {
      const {
        imported
      } = specifier;
      if (importedIsString) {
        throw this.raise(_parseError.Errors.ImportBindingIsString, {
          at: specifier,
          importName: imported.value
        });
      }
      this.checkReservedWord(imported.name, specifier.loc.start, true, true);
      if (!specifier.local) {
        specifier.local = (0, _node.cloneIdentifier)(imported);
      }
    }
    return this.finishImportSpecifier(specifier, "ImportSpecifier", bindingType);
  }
  isThisParam(param) {
    return param.type === "Identifier" && param.name === "this";
  }
}
exports.default = StatementParser;

//# sourceMappingURL=statement.js.map
