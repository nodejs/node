"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;
var _scope = require("../../util/scope");
var _scopeflags = require("../../util/scopeflags");
var _parseError = require("../../parse-error");
class TypeScriptScope extends _scope.Scope {
  constructor(...args) {
    super(...args);
    this.types = new Set();
    this.enums = new Set();
    this.constEnums = new Set();
    this.classes = new Set();
    this.exportOnlyBindings = new Set();
  }
}
class TypeScriptScopeHandler extends _scope.default {
  constructor(...args) {
    super(...args);
    this.importsStack = [];
  }
  createScope(flags) {
    this.importsStack.push(new Set());
    return new TypeScriptScope(flags);
  }
  enter(flags) {
    if (flags == _scopeflags.SCOPE_TS_MODULE) {
      this.importsStack.push(new Set());
    }
    super.enter(flags);
  }
  exit() {
    const flags = super.exit();
    if (flags == _scopeflags.SCOPE_TS_MODULE) {
      this.importsStack.pop();
    }
    return flags;
  }
  hasImport(name, allowShadow) {
    const len = this.importsStack.length;
    if (this.importsStack[len - 1].has(name)) {
      return true;
    }
    if (!allowShadow && len > 1) {
      for (let i = 0; i < len - 1; i++) {
        if (this.importsStack[i].has(name)) return true;
      }
    }
    return false;
  }
  declareName(name, bindingType, loc) {
    if (bindingType & _scopeflags.BIND_FLAGS_TS_IMPORT) {
      if (this.hasImport(name, true)) {
        this.parser.raise(_parseError.Errors.VarRedeclaration, {
          at: loc,
          identifierName: name
        });
      }
      this.importsStack[this.importsStack.length - 1].add(name);
      return;
    }
    const scope = this.currentScope();
    if (bindingType & _scopeflags.BIND_FLAGS_TS_EXPORT_ONLY) {
      this.maybeExportDefined(scope, name);
      scope.exportOnlyBindings.add(name);
      return;
    }
    super.declareName(name, bindingType, loc);
    if (bindingType & _scopeflags.BIND_KIND_TYPE) {
      if (!(bindingType & _scopeflags.BIND_KIND_VALUE)) {
        this.checkRedeclarationInScope(scope, name, bindingType, loc);
        this.maybeExportDefined(scope, name);
      }
      scope.types.add(name);
    }
    if (bindingType & _scopeflags.BIND_FLAGS_TS_ENUM) scope.enums.add(name);
    if (bindingType & _scopeflags.BIND_FLAGS_TS_CONST_ENUM) scope.constEnums.add(name);
    if (bindingType & _scopeflags.BIND_FLAGS_CLASS) scope.classes.add(name);
  }
  isRedeclaredInScope(scope, name, bindingType) {
    if (scope.enums.has(name)) {
      if (bindingType & _scopeflags.BIND_FLAGS_TS_ENUM) {
        const isConst = !!(bindingType & _scopeflags.BIND_FLAGS_TS_CONST_ENUM);
        const wasConst = scope.constEnums.has(name);
        return isConst !== wasConst;
      }
      return true;
    }
    if (bindingType & _scopeflags.BIND_FLAGS_CLASS && scope.classes.has(name)) {
      if (scope.lexical.has(name)) {
        return !!(bindingType & _scopeflags.BIND_KIND_VALUE);
      } else {
        return false;
      }
    }
    if (bindingType & _scopeflags.BIND_KIND_TYPE && scope.types.has(name)) {
      return true;
    }
    return super.isRedeclaredInScope(scope, name, bindingType);
  }
  checkLocalExport(id) {
    const {
      name
    } = id;
    if (this.hasImport(name)) return;
    const len = this.scopeStack.length;
    for (let i = len - 1; i >= 0; i--) {
      const scope = this.scopeStack[i];
      if (scope.types.has(name) || scope.exportOnlyBindings.has(name)) return;
    }
    super.checkLocalExport(id);
  }
}
exports.default = TypeScriptScopeHandler;

//# sourceMappingURL=scope.js.map
