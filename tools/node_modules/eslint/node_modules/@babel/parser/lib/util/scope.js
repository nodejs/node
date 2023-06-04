"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = exports.Scope = void 0;
var _scopeflags = require("./scopeflags");
var _parseError = require("../parse-error");
class Scope {
  constructor(flags) {
    this.var = new Set();
    this.lexical = new Set();
    this.functions = new Set();
    this.flags = flags;
  }
}
exports.Scope = Scope;
class ScopeHandler {
  constructor(parser, inModule) {
    this.parser = void 0;
    this.scopeStack = [];
    this.inModule = void 0;
    this.undefinedExports = new Map();
    this.parser = parser;
    this.inModule = inModule;
  }
  get inTopLevel() {
    return (this.currentScope().flags & _scopeflags.SCOPE_PROGRAM) > 0;
  }
  get inFunction() {
    return (this.currentVarScopeFlags() & _scopeflags.SCOPE_FUNCTION) > 0;
  }
  get allowSuper() {
    return (this.currentThisScopeFlags() & _scopeflags.SCOPE_SUPER) > 0;
  }
  get allowDirectSuper() {
    return (this.currentThisScopeFlags() & _scopeflags.SCOPE_DIRECT_SUPER) > 0;
  }
  get inClass() {
    return (this.currentThisScopeFlags() & _scopeflags.SCOPE_CLASS) > 0;
  }
  get inClassAndNotInNonArrowFunction() {
    const flags = this.currentThisScopeFlags();
    return (flags & _scopeflags.SCOPE_CLASS) > 0 && (flags & _scopeflags.SCOPE_FUNCTION) === 0;
  }
  get inStaticBlock() {
    for (let i = this.scopeStack.length - 1;; i--) {
      const {
        flags
      } = this.scopeStack[i];
      if (flags & _scopeflags.SCOPE_STATIC_BLOCK) {
        return true;
      }
      if (flags & (_scopeflags.SCOPE_VAR | _scopeflags.SCOPE_CLASS)) {
        return false;
      }
    }
  }
  get inNonArrowFunction() {
    return (this.currentThisScopeFlags() & _scopeflags.SCOPE_FUNCTION) > 0;
  }
  get treatFunctionsAsVar() {
    return this.treatFunctionsAsVarInScope(this.currentScope());
  }
  createScope(flags) {
    return new Scope(flags);
  }
  enter(flags) {
    this.scopeStack.push(this.createScope(flags));
  }
  exit() {
    const scope = this.scopeStack.pop();
    return scope.flags;
  }
  treatFunctionsAsVarInScope(scope) {
    return !!(scope.flags & (_scopeflags.SCOPE_FUNCTION | _scopeflags.SCOPE_STATIC_BLOCK) || !this.parser.inModule && scope.flags & _scopeflags.SCOPE_PROGRAM);
  }
  declareName(name, bindingType, loc) {
    let scope = this.currentScope();
    if (bindingType & _scopeflags.BIND_SCOPE_LEXICAL || bindingType & _scopeflags.BIND_SCOPE_FUNCTION) {
      this.checkRedeclarationInScope(scope, name, bindingType, loc);
      if (bindingType & _scopeflags.BIND_SCOPE_FUNCTION) {
        scope.functions.add(name);
      } else {
        scope.lexical.add(name);
      }
      if (bindingType & _scopeflags.BIND_SCOPE_LEXICAL) {
        this.maybeExportDefined(scope, name);
      }
    } else if (bindingType & _scopeflags.BIND_SCOPE_VAR) {
      for (let i = this.scopeStack.length - 1; i >= 0; --i) {
        scope = this.scopeStack[i];
        this.checkRedeclarationInScope(scope, name, bindingType, loc);
        scope.var.add(name);
        this.maybeExportDefined(scope, name);
        if (scope.flags & _scopeflags.SCOPE_VAR) break;
      }
    }
    if (this.parser.inModule && scope.flags & _scopeflags.SCOPE_PROGRAM) {
      this.undefinedExports.delete(name);
    }
  }
  maybeExportDefined(scope, name) {
    if (this.parser.inModule && scope.flags & _scopeflags.SCOPE_PROGRAM) {
      this.undefinedExports.delete(name);
    }
  }
  checkRedeclarationInScope(scope, name, bindingType, loc) {
    if (this.isRedeclaredInScope(scope, name, bindingType)) {
      this.parser.raise(_parseError.Errors.VarRedeclaration, {
        at: loc,
        identifierName: name
      });
    }
  }
  isRedeclaredInScope(scope, name, bindingType) {
    if (!(bindingType & _scopeflags.BIND_KIND_VALUE)) return false;
    if (bindingType & _scopeflags.BIND_SCOPE_LEXICAL) {
      return scope.lexical.has(name) || scope.functions.has(name) || scope.var.has(name);
    }
    if (bindingType & _scopeflags.BIND_SCOPE_FUNCTION) {
      return scope.lexical.has(name) || !this.treatFunctionsAsVarInScope(scope) && scope.var.has(name);
    }
    return scope.lexical.has(name) && !(scope.flags & _scopeflags.SCOPE_SIMPLE_CATCH && scope.lexical.values().next().value === name) || !this.treatFunctionsAsVarInScope(scope) && scope.functions.has(name);
  }
  checkLocalExport(id) {
    const {
      name
    } = id;
    const topLevelScope = this.scopeStack[0];
    if (!topLevelScope.lexical.has(name) && !topLevelScope.var.has(name) && !topLevelScope.functions.has(name)) {
      this.undefinedExports.set(name, id.loc.start);
    }
  }
  currentScope() {
    return this.scopeStack[this.scopeStack.length - 1];
  }
  currentVarScopeFlags() {
    for (let i = this.scopeStack.length - 1;; i--) {
      const {
        flags
      } = this.scopeStack[i];
      if (flags & _scopeflags.SCOPE_VAR) {
        return flags;
      }
    }
  }
  currentThisScopeFlags() {
    for (let i = this.scopeStack.length - 1;; i--) {
      const {
        flags
      } = this.scopeStack[i];
      if (flags & (_scopeflags.SCOPE_VAR | _scopeflags.SCOPE_CLASS) && !(flags & _scopeflags.SCOPE_ARROW)) {
        return flags;
      }
    }
  }
}
exports.default = ScopeHandler;

//# sourceMappingURL=scope.js.map
