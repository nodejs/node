"use strict";

exports.__esModule = true;
exports.toComputedKey = toComputedKey;
exports.ensureBlock = ensureBlock;
exports.arrowFunctionToShadowed = arrowFunctionToShadowed;
exports.unwrapFunctionEnvironment = unwrapFunctionEnvironment;
exports.arrowFunctionToExpression = arrowFunctionToExpression;

var t = _interopRequireWildcard(require("@babel/types"));

var _helperFunctionName = _interopRequireDefault(require("@babel/helper-function-name"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

function toComputedKey() {
  var node = this.node;
  var key;

  if (this.isMemberExpression()) {
    key = node.property;
  } else if (this.isProperty() || this.isMethod()) {
    key = node.key;
  } else {
    throw new ReferenceError("todo");
  }

  if (!node.computed) {
    if (t.isIdentifier(key)) key = t.stringLiteral(key.name);
  }

  return key;
}

function ensureBlock() {
  var body = this.get("body");
  var bodyNode = body.node;

  if (Array.isArray(body)) {
    throw new Error("Can't convert array path to a block statement");
  }

  if (!bodyNode) {
    throw new Error("Can't convert node without a body");
  }

  if (body.isBlockStatement()) {
    return bodyNode;
  }

  var statements = [];
  var stringPath = "body";
  var key;
  var listKey;

  if (body.isStatement()) {
    listKey = "body";
    key = 0;
    statements.push(body.node);
  } else {
    stringPath += ".body.0";

    if (this.isFunction()) {
      key = "argument";
      statements.push(t.returnStatement(body.node));
    } else {
      key = "expression";
      statements.push(t.expressionStatement(body.node));
    }
  }

  this.node.body = t.blockStatement(statements);
  var parentPath = this.get(stringPath);
  body.setup(parentPath, listKey ? parentPath.node[listKey] : parentPath.node, listKey, key);
  return this.node;
}

function arrowFunctionToShadowed() {
  if (!this.isArrowFunctionExpression()) return;
  this.arrowFunctionToExpression();
}

function unwrapFunctionEnvironment() {
  if (!this.isArrowFunctionExpression() && !this.isFunctionExpression() && !this.isFunctionDeclaration()) {
    throw this.buildCodeFrameError("Can only unwrap the environment of a function.");
  }

  hoistFunctionEnvironment(this);
}

function arrowFunctionToExpression(_temp) {
  var _ref = _temp === void 0 ? {} : _temp,
      _ref$allowInsertArrow = _ref.allowInsertArrow,
      allowInsertArrow = _ref$allowInsertArrow === void 0 ? true : _ref$allowInsertArrow,
      _ref$specCompliant = _ref.specCompliant,
      specCompliant = _ref$specCompliant === void 0 ? false : _ref$specCompliant;

  if (!this.isArrowFunctionExpression()) {
    throw this.buildCodeFrameError("Cannot convert non-arrow function to a function expression.");
  }

  var thisBinding = hoistFunctionEnvironment(this, specCompliant, allowInsertArrow);
  this.ensureBlock();
  this.node.type = "FunctionExpression";

  if (specCompliant) {
    var checkBinding = thisBinding ? null : this.parentPath.scope.generateUidIdentifier("arrowCheckId");

    if (checkBinding) {
      this.parentPath.scope.push({
        id: checkBinding,
        init: t.objectExpression([])
      });
    }

    this.get("body").unshiftContainer("body", t.expressionStatement(t.callExpression(this.hub.file.addHelper("newArrowCheck"), [t.thisExpression(), checkBinding ? t.identifier(checkBinding.name) : t.identifier(thisBinding)])));
    this.replaceWith(t.callExpression(t.memberExpression((0, _helperFunctionName.default)(this, true) || this.node, t.identifier("bind")), [checkBinding ? t.identifier(checkBinding.name) : t.thisExpression()]));
  }
}

function hoistFunctionEnvironment(fnPath, specCompliant, allowInsertArrow) {
  if (specCompliant === void 0) {
    specCompliant = false;
  }

  if (allowInsertArrow === void 0) {
    allowInsertArrow = true;
  }

  var thisEnvFn = fnPath.findParent(function (p) {
    return p.isFunction() && !p.isArrowFunctionExpression() || p.isProgram() || p.isClassProperty({
      static: false
    });
  });
  var inConstructor = thisEnvFn && thisEnvFn.node.kind === "constructor";

  if (thisEnvFn.isClassProperty()) {
    throw fnPath.buildCodeFrameError("Unable to transform arrow inside class property");
  }

  var _getScopeInformation = getScopeInformation(fnPath),
      thisPaths = _getScopeInformation.thisPaths,
      argumentsPaths = _getScopeInformation.argumentsPaths,
      newTargetPaths = _getScopeInformation.newTargetPaths,
      superProps = _getScopeInformation.superProps,
      superCalls = _getScopeInformation.superCalls;

  if (inConstructor && superCalls.length > 0) {
    if (!allowInsertArrow) {
      throw superCalls[0].buildCodeFrameError("Unable to handle nested super() usage in arrow");
    }

    var allSuperCalls = [];
    thisEnvFn.traverse({
      Function: function Function(child) {
        if (child.isArrowFunctionExpression()) return;
        child.skip();
      },
      ClassProperty: function ClassProperty(child) {
        if (child.node.static) return;
        child.skip();
      },
      CallExpression: function CallExpression(child) {
        if (!child.get("callee").isSuper()) return;
        allSuperCalls.push(child);
      }
    });
    var superBinding = getSuperBinding(thisEnvFn);
    allSuperCalls.forEach(function (superCall) {
      return superCall.get("callee").replaceWith(t.identifier(superBinding));
    });
  }

  var thisBinding;

  if (thisPaths.length > 0 || specCompliant) {
    thisBinding = getThisBinding(thisEnvFn, inConstructor);

    if (!specCompliant || inConstructor && hasSuperClass(thisEnvFn)) {
      thisPaths.forEach(function (thisChild) {
        thisChild.replaceWith(thisChild.isJSX() ? t.jsxIdentifier(thisBinding) : t.identifier(thisBinding));
      });
      if (specCompliant) thisBinding = null;
    }
  }

  if (argumentsPaths.length > 0) {
    var argumentsBinding = getBinding(thisEnvFn, "arguments", function () {
      return t.identifier("arguments");
    });
    argumentsPaths.forEach(function (argumentsChild) {
      argumentsChild.replaceWith(t.identifier(argumentsBinding));
    });
  }

  if (newTargetPaths.length > 0) {
    var newTargetBinding = getBinding(thisEnvFn, "newtarget", function () {
      return t.metaProperty(t.identifier("new"), t.identifier("target"));
    });
    newTargetPaths.forEach(function (argumentsChild) {
      argumentsChild.replaceWith(t.identifier(newTargetBinding));
    });
  }

  if (superProps.length > 0) {
    if (!allowInsertArrow) {
      throw superProps[0].buildCodeFrameError("Unable to handle nested super.prop usage");
    }

    var flatSuperProps = superProps.reduce(function (acc, superProp) {
      return acc.concat(standardizeSuperProperty(superProp));
    }, []);
    flatSuperProps.forEach(function (superProp) {
      var key = superProp.node.computed ? "" : superProp.get("property").node.name;

      if (superProp.parentPath.isCallExpression({
        callee: superProp.node
      })) {
        var _superBinding = getSuperPropCallBinding(thisEnvFn, key);

        if (superProp.node.computed) {
          var prop = superProp.get("property").node;
          superProp.replaceWith(t.identifier(_superBinding));
          superProp.parentPath.node.arguments.unshift(prop);
        } else {
          superProp.replaceWith(t.identifier(_superBinding));
        }
      } else {
        var isAssignment = superProp.parentPath.isAssignmentExpression({
          left: superProp.node
        });

        var _superBinding2 = getSuperPropBinding(thisEnvFn, isAssignment, key);

        var args = [];

        if (superProp.node.computed) {
          args.push(superProp.get("property").node);
        }

        if (isAssignment) {
          var value = superProp.parentPath.node.right;
          args.push(value);
          superProp.parentPath.replaceWith(t.callExpression(t.identifier(_superBinding2), args));
        } else {
          superProp.replaceWith(t.callExpression(t.identifier(_superBinding2), args));
        }
      }
    });
  }

  return thisBinding;
}

function standardizeSuperProperty(superProp) {
  if (superProp.parentPath.isAssignmentExpression() && superProp.parentPath.node.operator !== "=") {
    var assignmentPath = superProp.parentPath;
    var op = assignmentPath.node.operator.slice(0, -1);
    var value = assignmentPath.node.right;
    assignmentPath.node.operator = "=";

    if (superProp.node.computed) {
      var tmp = superProp.scope.generateDeclaredUidIdentifier("tmp");
      assignmentPath.get("left").replaceWith(t.memberExpression(superProp.node.object, t.assignmentExpression("=", tmp, superProp.node.property), true));
      assignmentPath.get("right").replaceWith(t.binaryExpression(op, t.memberExpression(superProp.node.object, t.identifier(tmp.name), true), value));
    } else {
      assignmentPath.get("left").replaceWith(t.memberExpression(superProp.node.object, superProp.node.property));
      assignmentPath.get("right").replaceWith(t.binaryExpression(op, t.memberExpression(superProp.node.object, t.identifier(superProp.node.property.name)), value));
    }

    return [assignmentPath.get("left"), assignmentPath.get("right").get("left")];
  } else if (superProp.parentPath.isUpdateExpression()) {
    var updateExpr = superProp.parentPath;

    var _tmp = superProp.scope.generateDeclaredUidIdentifier("tmp");

    var computedKey = superProp.node.computed ? superProp.scope.generateDeclaredUidIdentifier("prop") : null;
    var parts = [t.assignmentExpression("=", _tmp, t.memberExpression(superProp.node.object, computedKey ? t.assignmentExpression("=", computedKey, superProp.node.property) : superProp.node.property, superProp.node.computed)), t.assignmentExpression("=", t.memberExpression(superProp.node.object, computedKey ? t.identifier(computedKey.name) : superProp.node.property, superProp.node.computed), t.binaryExpression("+", t.identifier(_tmp.name), t.numericLiteral(1)))];

    if (!superProp.parentPath.node.prefix) {
      parts.push(t.identifier(_tmp.name));
    }

    updateExpr.replaceWith(t.sequenceExpression(parts));
    var left = updateExpr.get("expressions.0.right");
    var right = updateExpr.get("expressions.1.left");
    return [left, right];
  }

  return [superProp];
}

function hasSuperClass(thisEnvFn) {
  return thisEnvFn.isClassMethod() && !!thisEnvFn.parentPath.parentPath.node.superClass;
}

function getThisBinding(thisEnvFn, inConstructor) {
  return getBinding(thisEnvFn, "this", function (thisBinding) {
    if (!inConstructor || !hasSuperClass(thisEnvFn)) return t.thisExpression();
    var supers = new WeakSet();
    thisEnvFn.traverse({
      Function: function Function(child) {
        if (child.isArrowFunctionExpression()) return;
        child.skip();
      },
      ClassProperty: function ClassProperty(child) {
        if (child.node.static) return;
        child.skip();
      },
      CallExpression: function CallExpression(child) {
        if (!child.get("callee").isSuper()) return;
        if (supers.has(child.node)) return;
        supers.add(child.node);
        child.replaceWith(t.assignmentExpression("=", t.identifier(thisBinding), child.node));
      }
    });
  });
}

function getSuperBinding(thisEnvFn) {
  return getBinding(thisEnvFn, "supercall", function () {
    var argsBinding = thisEnvFn.scope.generateUidIdentifier("args");
    return t.arrowFunctionExpression([t.restElement(argsBinding)], t.callExpression(t.super(), [t.spreadElement(t.identifier(argsBinding.name))]));
  });
}

function getSuperPropCallBinding(thisEnvFn, propName) {
  return getBinding(thisEnvFn, "superprop_call:" + (propName || ""), function () {
    var argsBinding = thisEnvFn.scope.generateUidIdentifier("args");
    var argsList = [t.restElement(argsBinding)];
    var fnBody;

    if (propName) {
      fnBody = t.callExpression(t.memberExpression(t.super(), t.identifier(propName)), [t.spreadElement(t.identifier(argsBinding.name))]);
    } else {
      var method = thisEnvFn.scope.generateUidIdentifier("prop");
      argsList.unshift(method);
      fnBody = t.callExpression(t.memberExpression(t.super(), t.identifier(method.name), true), [t.spreadElement(t.identifier(argsBinding.name))]);
    }

    return t.arrowFunctionExpression(argsList, fnBody);
  });
}

function getSuperPropBinding(thisEnvFn, isAssignment, propName) {
  var op = isAssignment ? "set" : "get";
  return getBinding(thisEnvFn, "superprop_" + op + ":" + (propName || ""), function () {
    var argsList = [];
    var fnBody;

    if (propName) {
      fnBody = t.memberExpression(t.super(), t.identifier(propName));
    } else {
      var method = thisEnvFn.scope.generateUidIdentifier("prop");
      argsList.unshift(method);
      fnBody = t.memberExpression(t.super(), t.identifier(method.name), true);
    }

    if (isAssignment) {
      var valueIdent = thisEnvFn.scope.generateUidIdentifier("value");
      argsList.push(valueIdent);
      fnBody = t.assignmentExpression("=", fnBody, t.identifier(valueIdent.name));
    }

    return t.arrowFunctionExpression(argsList, fnBody);
  });
}

function getBinding(thisEnvFn, key, init) {
  var cacheKey = "binding:" + key;
  var data = thisEnvFn.getData(cacheKey);

  if (!data) {
    var id = thisEnvFn.scope.generateUidIdentifier(key);
    data = id.name;
    thisEnvFn.setData(cacheKey, data);
    thisEnvFn.scope.push({
      id: id,
      init: init(data)
    });
  }

  return data;
}

function getScopeInformation(fnPath) {
  var thisPaths = [];
  var argumentsPaths = [];
  var newTargetPaths = [];
  var superProps = [];
  var superCalls = [];
  fnPath.traverse({
    ClassProperty: function ClassProperty(child) {
      if (child.node.static) return;
      child.skip();
    },
    Function: function Function(child) {
      if (child.isArrowFunctionExpression()) return;
      child.skip();
    },
    ThisExpression: function ThisExpression(child) {
      thisPaths.push(child);
    },
    JSXIdentifier: function JSXIdentifier(child) {
      if (child.node.name !== "this") return;

      if (!child.parentPath.isJSXMemberExpression({
        object: child.node
      }) && !child.parentPath.isJSXOpeningElement({
        name: child.node
      })) {
        return;
      }

      thisPaths.push(child);
    },
    CallExpression: function CallExpression(child) {
      if (child.get("callee").isSuper()) superCalls.push(child);
    },
    MemberExpression: function MemberExpression(child) {
      if (child.get("object").isSuper()) superProps.push(child);
    },
    ReferencedIdentifier: function ReferencedIdentifier(child) {
      if (child.node.name !== "arguments") return;
      argumentsPaths.push(child);
    },
    MetaProperty: function MetaProperty(child) {
      if (!child.get("meta").isIdentifier({
        name: "new"
      })) return;
      if (!child.get("property").isIdentifier({
        name: "target"
      })) return;
      newTargetPaths.push(child);
    }
  });
  return {
    thisPaths: thisPaths,
    argumentsPaths: argumentsPaths,
    newTargetPaths: newTargetPaths,
    superProps: superProps,
    superCalls: superCalls
  };
}