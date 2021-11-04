"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.environmentVisitor = exports.default = void 0;
exports.skipAllButComputedKey = skipAllButComputedKey;

var _traverse = require("@babel/traverse");

var _helperMemberExpressionToFunctions = require("@babel/helper-member-expression-to-functions");

var _helperOptimiseCallExpression = require("@babel/helper-optimise-call-expression");

var _t = require("@babel/types");

const {
  VISITOR_KEYS,
  assignmentExpression,
  booleanLiteral,
  callExpression,
  cloneNode,
  identifier,
  memberExpression,
  sequenceExpression,
  staticBlock,
  stringLiteral,
  thisExpression
} = _t;

function getPrototypeOfExpression(objectRef, isStatic, file, isPrivateMethod) {
  objectRef = cloneNode(objectRef);
  const targetRef = isStatic || isPrivateMethod ? objectRef : memberExpression(objectRef, identifier("prototype"));
  return callExpression(file.addHelper("getPrototypeOf"), [targetRef]);
}

function skipAllButComputedKey(path) {
  if (!path.node.computed) {
    path.skip();
    return;
  }

  const keys = VISITOR_KEYS[path.type];

  for (const key of keys) {
    if (key !== "key") path.skipKey(key);
  }
}

const environmentVisitor = {
  [`${staticBlock ? "StaticBlock|" : ""}ClassPrivateProperty|TypeAnnotation`](path) {
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
      return cloneNode(this.memoiser.get(property));
    }

    if (computed) {
      return cloneNode(property);
    }

    return stringLiteral(property.name);
  },

  get(superMember) {
    return this._get(superMember, this._getThisRefs());
  },

  _get(superMember, thisRefs) {
    const proto = getPrototypeOfExpression(this.getObjectRef(), this.isStatic, this.file, this.isPrivateMethod);
    return callExpression(this.file.addHelper("get"), [thisRefs.memo ? sequenceExpression([thisRefs.memo, proto]) : proto, this.prop(superMember), thisRefs.this]);
  },

  _getThisRefs() {
    if (!this.isDerivedConstructor) {
      return {
        this: thisExpression()
      };
    }

    const thisRef = this.scope.generateDeclaredUidIdentifier("thisSuper");
    return {
      memo: assignmentExpression("=", thisRef, thisExpression()),
      this: cloneNode(thisRef)
    };
  },

  set(superMember, value) {
    const thisRefs = this._getThisRefs();

    const proto = getPrototypeOfExpression(this.getObjectRef(), this.isStatic, this.file, this.isPrivateMethod);
    return callExpression(this.file.addHelper("set"), [thisRefs.memo ? sequenceExpression([thisRefs.memo, proto]) : proto, this.prop(superMember), value, thisRefs.this, booleanLiteral(superMember.isInStrictMode())]);
  },

  destructureSet(superMember) {
    throw superMember.buildCodeFrameError(`Destructuring to a super field is not supported yet.`);
  },

  call(superMember, args) {
    const thisRefs = this._getThisRefs();

    return (0, _helperOptimiseCallExpression.default)(this._get(superMember, thisRefs), cloneNode(thisRefs.this), args, false);
  },

  optionalCall(superMember, args) {
    const thisRefs = this._getThisRefs();

    return (0, _helperOptimiseCallExpression.default)(this._get(superMember, thisRefs), cloneNode(thisRefs.this), args, true);
  }

};
const looseHandlers = Object.assign({}, specHandlers, {
  prop(superMember) {
    const {
      property
    } = superMember.node;

    if (this.memoiser.has(property)) {
      return cloneNode(this.memoiser.get(property));
    }

    return cloneNode(property);
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

      object = (_getSuperRef = getSuperRef()) != null ? _getSuperRef : memberExpression(identifier("Function"), identifier("prototype"));
    } else {
      var _getSuperRef2;

      object = memberExpression((_getSuperRef2 = getSuperRef()) != null ? _getSuperRef2 : identifier("Object"), identifier("prototype"));
    }

    return memberExpression(object, prop, computed);
  },

  set(superMember, value) {
    const {
      computed
    } = superMember.node;
    const prop = this.prop(superMember);
    return assignmentExpression("=", memberExpression(thisExpression(), prop, computed), value);
  },

  destructureSet(superMember) {
    const {
      computed
    } = superMember.node;
    const prop = this.prop(superMember);
    return memberExpression(thisExpression(), prop, computed);
  },

  call(superMember, args) {
    return (0, _helperOptimiseCallExpression.default)(this.get(superMember), thisExpression(), args, false);
  },

  optionalCall(superMember, args) {
    return (0, _helperOptimiseCallExpression.default)(this.get(superMember), thisExpression(), args, true);
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
    return cloneNode(this.opts.objectRef || this.opts.getObjectRef());
  }

  getSuperRef() {
    if (this.opts.superRef) return cloneNode(this.opts.superRef);
    if (this.opts.getSuperRef) return cloneNode(this.opts.getSuperRef());
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
      getSuperRef: this.getSuperRef.bind(this),
      boundGet: handler.get
    }, handler));
  }

}

exports.default = ReplaceSupers;