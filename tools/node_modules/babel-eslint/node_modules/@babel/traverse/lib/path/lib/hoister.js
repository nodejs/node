"use strict";

exports.__esModule = true;
exports.default = void 0;

var t = _interopRequireWildcard(require("@babel/types"));

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

var referenceVisitor = {
  ReferencedIdentifier: function ReferencedIdentifier(path, state) {
    if (path.isJSXIdentifier() && t.react.isCompatTag(path.node.name) && !path.parentPath.isJSXMemberExpression()) {
      return;
    }

    if (path.node.name === "this") {
      var scope = path.scope;

      do {
        if (scope.path.isFunction() && !scope.path.isArrowFunctionExpression()) {
          break;
        }
      } while (scope = scope.parent);

      if (scope) state.breakOnScopePaths.push(scope.path);
    }

    var binding = path.scope.getBinding(path.node.name);
    if (!binding) return;
    if (binding !== state.scope.getBinding(path.node.name)) return;
    state.bindings[path.node.name] = binding;
  }
};

var PathHoister = function () {
  function PathHoister(path, scope) {
    this.breakOnScopePaths = [];
    this.bindings = {};
    this.scopes = [];
    this.scope = scope;
    this.path = path;
    this.attachAfter = false;
  }

  var _proto = PathHoister.prototype;

  _proto.isCompatibleScope = function isCompatibleScope(scope) {
    for (var key in this.bindings) {
      var binding = this.bindings[key];

      if (!scope.bindingIdentifierEquals(key, binding.identifier)) {
        return false;
      }
    }

    return true;
  };

  _proto.getCompatibleScopes = function getCompatibleScopes() {
    var scope = this.path.scope;

    do {
      if (this.isCompatibleScope(scope)) {
        this.scopes.push(scope);
      } else {
        break;
      }

      if (this.breakOnScopePaths.indexOf(scope.path) >= 0) {
        break;
      }
    } while (scope = scope.parent);
  };

  _proto.getAttachmentPath = function getAttachmentPath() {
    var path = this._getAttachmentPath();

    if (!path) return;
    var targetScope = path.scope;

    if (targetScope.path === path) {
      targetScope = path.scope.parent;
    }

    if (targetScope.path.isProgram() || targetScope.path.isFunction()) {
      for (var name in this.bindings) {
        if (!targetScope.hasOwnBinding(name)) continue;
        var binding = this.bindings[name];

        if (binding.kind === "param" || binding.path.parentKey === "params") {
          continue;
        }

        var bindingParentPath = this.getAttachmentParentForPath(binding.path);

        if (bindingParentPath.key >= path.key) {
          this.attachAfter = true;
          path = binding.path;
          var _arr = binding.constantViolations;

          for (var _i = 0; _i < _arr.length; _i++) {
            var violationPath = _arr[_i];

            if (this.getAttachmentParentForPath(violationPath).key > path.key) {
              path = violationPath;
            }
          }
        }
      }
    }

    return path;
  };

  _proto._getAttachmentPath = function _getAttachmentPath() {
    var scopes = this.scopes;
    var scope = scopes.pop();
    if (!scope) return;

    if (scope.path.isFunction()) {
      if (this.hasOwnParamBindings(scope)) {
        if (this.scope === scope) return;
        var bodies = scope.path.get("body").get("body");

        for (var i = 0; i < bodies.length; i++) {
          if (bodies[i].node._blockHoist) continue;
          return bodies[i];
        }
      } else {
        return this.getNextScopeAttachmentParent();
      }
    } else if (scope.path.isProgram()) {
      return this.getNextScopeAttachmentParent();
    }
  };

  _proto.getNextScopeAttachmentParent = function getNextScopeAttachmentParent() {
    var scope = this.scopes.pop();
    if (scope) return this.getAttachmentParentForPath(scope.path);
  };

  _proto.getAttachmentParentForPath = function getAttachmentParentForPath(path) {
    do {
      if (!path.parentPath || Array.isArray(path.container) && path.isStatement()) {
        return path;
      }
    } while (path = path.parentPath);
  };

  _proto.hasOwnParamBindings = function hasOwnParamBindings(scope) {
    for (var name in this.bindings) {
      if (!scope.hasOwnBinding(name)) continue;
      var binding = this.bindings[name];
      if (binding.kind === "param" && binding.constant) return true;
    }

    return false;
  };

  _proto.run = function run() {
    this.path.traverse(referenceVisitor, this);
    this.getCompatibleScopes();
    var attachTo = this.getAttachmentPath();
    if (!attachTo) return;
    if (attachTo.getFunctionParent() === this.path.getFunctionParent()) return;
    var uid = attachTo.scope.generateUidIdentifier("ref");
    var declarator = t.variableDeclarator(uid, this.path.node);
    var insertFn = this.attachAfter ? "insertAfter" : "insertBefore";
    attachTo[insertFn]([attachTo.isVariableDeclarator() ? declarator : t.variableDeclaration("var", [declarator])]);
    var parent = this.path.parentPath;

    if (parent.isJSXElement() && this.path.container === parent.node.children) {
      uid = t.JSXExpressionContainer(uid);
    }

    this.path.replaceWith(uid);
  };

  return PathHoister;
}();

exports.default = PathHoister;