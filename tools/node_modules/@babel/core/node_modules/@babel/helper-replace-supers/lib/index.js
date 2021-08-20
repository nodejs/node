"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.skipAllButComputedKey = skipAllButComputedKey;
exports.default = exports.environmentVisitor = void 0;

var _traverse = require("@babel/traverse");

var _helperMemberExpressionToFunctions = require("@babel/helper-member-expression-to-functions");

var _helperOptimiseCallExpression = require("@babel/helper-optimise-call-expression");

var t = require("@babel/types");

function getPrototypeOfExpression(objectRef, isStatic, file, isPrivateMethod) {
  objectRef = t.cloneNode(objectRef);
  const targetRef = isStatic || isPrivateMethod ? objectRef : t.memberExpression(objectRef, t.identifier("prototype"));
  return t.callExpression(file.addHelper("getPrototypeOf"), [targetRef]);
}

function skipAllButComputedKey(path) {
  if (!path.node.computed) {
    path.skip();
    return;
  }

  const keys = t.VISITOR_KEYS[path.type];

  for (const key of keys) {
    if (key !== "key") path.skipKey(key);
  }
}

const environmentVisitor = {
  [`${t.staticBlock ? "StaticBlock|" : ""}ClassPrivateProperty|TypeAnnotation`](path) {
    path.skip();
  },

  Function(path) {
    if (path.isMethod()) return;
    if (path.isArrowFunctionExpression()) return;
    path.skip();
  },

  "Method|ClassProperty"(path) {
    skipAllButComputedKey(path);
  }

};
exports.environmentVisitor = environmentVisitor;

const visitor = _traverse.default.visitors.merge([environmentVisitor, {
  Super(path, state) {
    const {
      node,
      parentPath
    } = path;
    if (!parentPath.isMemberExpression({
      object: node
    })) return;
    state.handle(parentPath);
  }

}]);

const unshadowSuperBindingVisitor = _traverse.default.visitors.merge([environmentVisitor, {
  Scopable(path, {
    refName
  }) {
    const binding = path.scope.getOwnBinding(refName);

    if (binding && binding.identifier.name === refName) {
      path.scope.rename(refName);
    }
  }

}]);

const specHandlers = {
  memoise(superMember, count) {
    const {
      scope,
      node
    } = superMember;
    const {
      computed,
      property
    } = node;

    if (!computed) {
      return;
    }

    const memo = scope.maybeGenerateMemoised(property);

    if (!memo) {
      return;
    }

    this.memoiser.set(property, memo, count);
  },

  prop(superMember) {
    const {
      computed,
      property
    } = superMember.node;

    if (this.memoiser.has(property)) {
      return t.cloneNode(this.memoiser.get(property));
    }

    if (computed) {
      return t.cloneNode(property);
    }

    return t.stringLiteral(property.name);
  },

  get(superMember) {
    return this._get(superMember, this._getThisRefs());
  },

  _get(superMember, thisRefs) {
    const proto = getPrototypeOfExpression(this.getObjectRef(), this.isStatic, this.file, this.isPrivateMethod);
    return t.callExpression(this.file.addHelper("get"), [thisRefs.memo ? t.sequenceExpression([thisRefs.memo, proto]) : proto, this.prop(superMember), thisRefs.this]);
  },

  _getThisRefs() {
    if (!this.isDerivedConstructor) {
      return {
        this: t.thisExpression()
      };
    }

    const thisRef = this.scope.generateDeclaredUidIdentifier("thisSuper");
    return {
      memo: t.assignmentExpression("=", thisRef, t.thisExpression()),
      this: t.cloneNode(thisRef)
    };
  },

  set(superMember, value) {
    const thisRefs = this._getThisRefs();

    const proto = getPrototypeOfExpression(this.getObjectRef(), this.isStatic, this.file, this.isPrivateMethod);
    return t.callExpression(this.file.addHelper("set"), [thisRefs.memo ? t.sequenceExpression([thisRefs.memo, proto]) : proto, this.prop(superMember), value, thisRefs.this, t.booleanLiteral(superMember.isInStrictMode())]);
  },

  destructureSet(superMember) {
    throw superMember.buildCodeFrameError(`Destructuring to a super field is not supported yet.`);
  },

  call(superMember, args) {
    const thisRefs = this._getThisRefs();

    return (0, _helperOptimiseCallExpression.default)(this._get(superMember, thisRefs), t.cloneNode(thisRefs.this), args, false);
  },

  optionalCall(superMember, args) {
    const thisRefs = this._getThisRefs();

    return (0, _helperOptimiseCallExpression.default)(this._get(superMember, thisRefs), t.cloneNode(thisRefs.this), args, true);
  }

};
const looseHandlers = Object.assign({}, specHandlers, {
  prop(superMember) {
    const {
      property
    } = superMember.node;

    if (this.memoiser.has(property)) {
      return t.cloneNode(this.memoiser.get(property));
    }

    return t.cloneNode(property);
  },

  get(superMember) {
    const {
      isStatic,
      getSuperRef
    } = this;
    const {
      computed
    } = superMember.node;
    const prop = this.prop(superMember);
    let object;

    if (isStatic) {
      var _getSuperRef;

      object = (_getSuperRef = getSuperRef()) != null ? _getSuperRef : t.memberExpression(t.identifier("Function"), t.identifier("prototype"));
    } else {
      var _getSuperRef2;

      object = t.memberExpression((_getSuperRef2 = getSuperRef()) != null ? _getSuperRef2 : t.identifier("Object"), t.identifier("prototype"));
    }

    return t.memberExpression(object, prop, computed);
  },

  set(superMember, value) {
    const {
      computed
    } = superMember.node;
    const prop = this.prop(superMember);
    return t.assignmentExpression("=", t.memberExpression(t.thisExpression(), prop, computed), value);
  },

  destructureSet(superMember) {
    const {
      computed
    } = superMember.node;
    const prop = this.prop(superMember);
    return t.memberExpression(t.thisExpression(), prop, computed);
  },

  call(superMember, args) {
    return (0, _helperOptimiseCallExpression.default)(this.get(superMember), t.thisExpression(), args, false);
  },

  optionalCall(superMember, args) {
    return (0, _helperOptimiseCallExpression.default)(this.get(superMember), t.thisExpression(), args, true);
  }

});

class ReplaceSupers {
  constructor(opts) {
    var _opts$constantSuper;

    const path = opts.methodPath;
    this.methodPath = path;
    this.isDerivedConstructor = path.isClassMethod({
      kind: "constructor"
    }) && !!opts.superRef;
    this.isStatic = path.isObjectMethod() || path.node.static || (path.isStaticBlock == null ? void 0 : path.isStaticBlock());
    this.isPrivateMethod = path.isPrivate() && path.isMethod();
    this.file = opts.file;
    this.constantSuper = (_opts$constantSuper = opts.constantSuper) != null ? _opts$constantSuper : opts.isLoose;
    this.opts = opts;
  }

  getObjectRef() {
    return t.cloneNode(this.opts.objectRef || this.opts.getObjectRef());
  }

  getSuperRef() {
    if (this.opts.superRef) return t.cloneNode(this.opts.superRef);
    if (this.opts.getSuperRef) return t.cloneNode(this.opts.getSuperRef());
  }

  replace() {
    if (this.opts.refToPreserve) {
      this.methodPath.traverse(unshadowSuperBindingVisitor, {
        refName: this.opts.refToPreserve.name
      });
    }

    const handler = this.constantSuper ? looseHandlers : specHandlers;
    (0, _helperMemberExpressionToFunctions.default)(this.methodPath, visitor, Object.assign({
      file: this.file,
      scope: this.methodPath.scope,
      isDerivedConstructor: this.isDerivedConstructor,
      isStatic: this.isStatic,
      isPrivateMethod: this.isPrivateMethod,
      getObjectRef: this.getObjectRef.bind(this),
      getSuperRef: this.getSuperRef.bind(this)
    }, handler));
  }

}

exports.default = ReplaceSupers;