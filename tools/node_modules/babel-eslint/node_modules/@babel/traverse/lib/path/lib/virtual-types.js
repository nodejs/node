"use strict";

exports.__esModule = true;
exports.ForAwaitStatement = exports.NumericLiteralTypeAnnotation = exports.ExistentialTypeParam = exports.SpreadProperty = exports.RestProperty = exports.Flow = exports.Pure = exports.Generated = exports.User = exports.Var = exports.BlockScoped = exports.Referenced = exports.Scope = exports.Expression = exports.Statement = exports.BindingIdentifier = exports.ReferencedMemberExpression = exports.ReferencedIdentifier = void 0;

var t = _interopRequireWildcard(require("@babel/types"));

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

var ReferencedIdentifier = {
  types: ["Identifier", "JSXIdentifier"],
  checkPath: function checkPath(_ref, opts) {
    var node = _ref.node,
        parent = _ref.parent;

    if (!t.isIdentifier(node, opts) && !t.isJSXMemberExpression(parent, opts)) {
      if (t.isJSXIdentifier(node, opts)) {
        if (t.react.isCompatTag(node.name)) return false;
      } else {
        return false;
      }
    }

    return t.isReferenced(node, parent);
  }
};
exports.ReferencedIdentifier = ReferencedIdentifier;
var ReferencedMemberExpression = {
  types: ["MemberExpression"],
  checkPath: function checkPath(_ref2) {
    var node = _ref2.node,
        parent = _ref2.parent;
    return t.isMemberExpression(node) && t.isReferenced(node, parent);
  }
};
exports.ReferencedMemberExpression = ReferencedMemberExpression;
var BindingIdentifier = {
  types: ["Identifier"],
  checkPath: function checkPath(_ref3) {
    var node = _ref3.node,
        parent = _ref3.parent;
    return t.isIdentifier(node) && t.isBinding(node, parent);
  }
};
exports.BindingIdentifier = BindingIdentifier;
var Statement = {
  types: ["Statement"],
  checkPath: function checkPath(_ref4) {
    var node = _ref4.node,
        parent = _ref4.parent;

    if (t.isStatement(node)) {
      if (t.isVariableDeclaration(node)) {
        if (t.isForXStatement(parent, {
          left: node
        })) return false;
        if (t.isForStatement(parent, {
          init: node
        })) return false;
      }

      return true;
    } else {
      return false;
    }
  }
};
exports.Statement = Statement;
var Expression = {
  types: ["Expression"],
  checkPath: function checkPath(path) {
    if (path.isIdentifier()) {
      return path.isReferencedIdentifier();
    } else {
      return t.isExpression(path.node);
    }
  }
};
exports.Expression = Expression;
var Scope = {
  types: ["Scopable"],
  checkPath: function checkPath(path) {
    return t.isScope(path.node, path.parent);
  }
};
exports.Scope = Scope;
var Referenced = {
  checkPath: function checkPath(path) {
    return t.isReferenced(path.node, path.parent);
  }
};
exports.Referenced = Referenced;
var BlockScoped = {
  checkPath: function checkPath(path) {
    return t.isBlockScoped(path.node);
  }
};
exports.BlockScoped = BlockScoped;
var Var = {
  types: ["VariableDeclaration"],
  checkPath: function checkPath(path) {
    return t.isVar(path.node);
  }
};
exports.Var = Var;
var User = {
  checkPath: function checkPath(path) {
    return path.node && !!path.node.loc;
  }
};
exports.User = User;
var Generated = {
  checkPath: function checkPath(path) {
    return !path.isUser();
  }
};
exports.Generated = Generated;
var Pure = {
  checkPath: function checkPath(path, opts) {
    return path.scope.isPure(path.node, opts);
  }
};
exports.Pure = Pure;
var Flow = {
  types: ["Flow", "ImportDeclaration", "ExportDeclaration", "ImportSpecifier"],
  checkPath: function checkPath(_ref5) {
    var node = _ref5.node;

    if (t.isFlow(node)) {
      return true;
    } else if (t.isImportDeclaration(node)) {
      return node.importKind === "type" || node.importKind === "typeof";
    } else if (t.isExportDeclaration(node)) {
      return node.exportKind === "type";
    } else if (t.isImportSpecifier(node)) {
      return node.importKind === "type" || node.importKind === "typeof";
    } else {
      return false;
    }
  }
};
exports.Flow = Flow;
var RestProperty = {
  types: ["RestElement"],
  checkPath: function checkPath(path) {
    return path.parentPath && path.parentPath.isObjectPattern();
  }
};
exports.RestProperty = RestProperty;
var SpreadProperty = {
  types: ["RestElement"],
  checkPath: function checkPath(path) {
    return path.parentPath && path.parentPath.isObjectExpression();
  }
};
exports.SpreadProperty = SpreadProperty;
var ExistentialTypeParam = {
  types: ["ExistsTypeAnnotation"]
};
exports.ExistentialTypeParam = ExistentialTypeParam;
var NumericLiteralTypeAnnotation = {
  types: ["NumberLiteralTypeAnnotation"]
};
exports.NumericLiteralTypeAnnotation = NumericLiteralTypeAnnotation;
var ForAwaitStatement = {
  types: ["ForOfStatement"],
  checkPath: function checkPath(_ref6) {
    var node = _ref6.node;
    return node.await === true;
  }
};
exports.ForAwaitStatement = ForAwaitStatement;