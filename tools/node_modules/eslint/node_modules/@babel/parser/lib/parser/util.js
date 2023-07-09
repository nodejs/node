"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = exports.ExpressionErrors = void 0;
var _types = require("../tokenizer/types");
var _tokenizer = require("../tokenizer");
var _whitespace = require("../util/whitespace");
var _identifier = require("../util/identifier");
var _classScope = require("../util/class-scope");
var _expressionScope = require("../util/expression-scope");
var _scopeflags = require("../util/scopeflags");
var _productionParameter = require("../util/production-parameter");
var _parseError = require("../parse-error");
class UtilParser extends _tokenizer.default {
  addExtra(node, key, value, enumerable = true) {
    if (!node) return;
    const extra = node.extra = node.extra || {};
    if (enumerable) {
      extra[key] = value;
    } else {
      Object.defineProperty(extra, key, {
        enumerable,
        value
      });
    }
  }
  isContextual(token) {
    return this.state.type === token && !this.state.containsEsc;
  }
  isUnparsedContextual(nameStart, name) {
    const nameEnd = nameStart + name.length;
    if (this.input.slice(nameStart, nameEnd) === name) {
      const nextCh = this.input.charCodeAt(nameEnd);
      return !((0, _identifier.isIdentifierChar)(nextCh) || (nextCh & 0xfc00) === 0xd800);
    }
    return false;
  }
  isLookaheadContextual(name) {
    const next = this.nextTokenStart();
    return this.isUnparsedContextual(next, name);
  }
  eatContextual(token) {
    if (this.isContextual(token)) {
      this.next();
      return true;
    }
    return false;
  }
  expectContextual(token, toParseError) {
    if (!this.eatContextual(token)) {
      if (toParseError != null) {
        throw this.raise(toParseError, {
          at: this.state.startLoc
        });
      }
      this.unexpected(null, token);
    }
  }
  canInsertSemicolon() {
    return this.match(137) || this.match(8) || this.hasPrecedingLineBreak();
  }
  hasPrecedingLineBreak() {
    return _whitespace.lineBreak.test(this.input.slice(this.state.lastTokEndLoc.index, this.state.start));
  }
  hasFollowingLineBreak() {
    _whitespace.skipWhiteSpaceToLineBreak.lastIndex = this.state.end;
    return _whitespace.skipWhiteSpaceToLineBreak.test(this.input);
  }
  isLineTerminator() {
    return this.eat(13) || this.canInsertSemicolon();
  }
  semicolon(allowAsi = true) {
    if (allowAsi ? this.isLineTerminator() : this.eat(13)) return;
    this.raise(_parseError.Errors.MissingSemicolon, {
      at: this.state.lastTokEndLoc
    });
  }
  expect(type, loc) {
    this.eat(type) || this.unexpected(loc, type);
  }
  tryParse(fn, oldState = this.state.clone()) {
    const abortSignal = {
      node: null
    };
    try {
      const node = fn((node = null) => {
        abortSignal.node = node;
        throw abortSignal;
      });
      if (this.state.errors.length > oldState.errors.length) {
        const failState = this.state;
        this.state = oldState;
        this.state.tokensLength = failState.tokensLength;
        return {
          node,
          error: failState.errors[oldState.errors.length],
          thrown: false,
          aborted: false,
          failState
        };
      }
      return {
        node,
        error: null,
        thrown: false,
        aborted: false,
        failState: null
      };
    } catch (error) {
      const failState = this.state;
      this.state = oldState;
      if (error instanceof SyntaxError) {
        return {
          node: null,
          error,
          thrown: true,
          aborted: false,
          failState
        };
      }
      if (error === abortSignal) {
        return {
          node: abortSignal.node,
          error: null,
          thrown: false,
          aborted: true,
          failState
        };
      }
      throw error;
    }
  }
  checkExpressionErrors(refExpressionErrors, andThrow) {
    if (!refExpressionErrors) return false;
    const {
      shorthandAssignLoc,
      doubleProtoLoc,
      privateKeyLoc,
      optionalParametersLoc
    } = refExpressionErrors;
    const hasErrors = !!shorthandAssignLoc || !!doubleProtoLoc || !!optionalParametersLoc || !!privateKeyLoc;
    if (!andThrow) {
      return hasErrors;
    }
    if (shorthandAssignLoc != null) {
      this.raise(_parseError.Errors.InvalidCoverInitializedName, {
        at: shorthandAssignLoc
      });
    }
    if (doubleProtoLoc != null) {
      this.raise(_parseError.Errors.DuplicateProto, {
        at: doubleProtoLoc
      });
    }
    if (privateKeyLoc != null) {
      this.raise(_parseError.Errors.UnexpectedPrivateField, {
        at: privateKeyLoc
      });
    }
    if (optionalParametersLoc != null) {
      this.unexpected(optionalParametersLoc);
    }
  }
  isLiteralPropertyName() {
    return (0, _types.tokenIsLiteralPropertyName)(this.state.type);
  }
  isPrivateName(node) {
    return node.type === "PrivateName";
  }
  getPrivateNameSV(node) {
    return node.id.name;
  }
  hasPropertyAsPrivateName(node) {
    return (node.type === "MemberExpression" || node.type === "OptionalMemberExpression") && this.isPrivateName(node.property);
  }
  isObjectProperty(node) {
    return node.type === "ObjectProperty";
  }
  isObjectMethod(node) {
    return node.type === "ObjectMethod";
  }
  initializeScopes(inModule = this.options.sourceType === "module") {
    const oldLabels = this.state.labels;
    this.state.labels = [];
    const oldExportedIdentifiers = this.exportedIdentifiers;
    this.exportedIdentifiers = new Set();
    const oldInModule = this.inModule;
    this.inModule = inModule;
    const oldScope = this.scope;
    const ScopeHandler = this.getScopeHandler();
    this.scope = new ScopeHandler(this, inModule);
    const oldProdParam = this.prodParam;
    this.prodParam = new _productionParameter.default();
    const oldClassScope = this.classScope;
    this.classScope = new _classScope.default(this);
    const oldExpressionScope = this.expressionScope;
    this.expressionScope = new _expressionScope.default(this);
    return () => {
      this.state.labels = oldLabels;
      this.exportedIdentifiers = oldExportedIdentifiers;
      this.inModule = oldInModule;
      this.scope = oldScope;
      this.prodParam = oldProdParam;
      this.classScope = oldClassScope;
      this.expressionScope = oldExpressionScope;
    };
  }
  enterInitialScopes() {
    let paramFlags = _productionParameter.PARAM;
    if (this.inModule) {
      paramFlags |= _productionParameter.PARAM_AWAIT;
    }
    this.scope.enter(_scopeflags.SCOPE_PROGRAM);
    this.prodParam.enter(paramFlags);
  }
  checkDestructuringPrivate(refExpressionErrors) {
    const {
      privateKeyLoc
    } = refExpressionErrors;
    if (privateKeyLoc !== null) {
      this.expectPlugin("destructuringPrivate", privateKeyLoc);
    }
  }
}
exports.default = UtilParser;
class ExpressionErrors {
  constructor() {
    this.shorthandAssignLoc = null;
    this.doubleProtoLoc = null;
    this.privateKeyLoc = null;
    this.optionalParametersLoc = null;
  }
}
exports.ExpressionErrors = ExpressionErrors;

//# sourceMappingURL=util.js.map
