'use strict';

Object.defineProperty(exports, '__esModule', { value: true });

var t = require('@babel/types');

function _interopNamespace(e) {
  if (e && e.__esModule) return e;
  var n = Object.create(null);
  if (e) {
    Object.keys(e).forEach(function (k) {
      if (k !== 'default') {
        var d = Object.getOwnPropertyDescriptor(e, k);
        Object.defineProperty(n, k, d.get ? d : {
          enumerable: true,
          get: function () {
            return e[k];
          }
        });
      }
    });
  }
  n['default'] = e;
  return Object.freeze(n);
}

var t__namespace = /*#__PURE__*/_interopNamespace(t);

function willPathCastToBoolean(path) {
  const maybeWrapped = path;
  const {
    node,
    parentPath
  } = maybeWrapped;

  if (parentPath.isLogicalExpression()) {
    const {
      operator,
      right
    } = parentPath.node;

    if (operator === "&&" || operator === "||" || operator === "??" && node === right) {
      return willPathCastToBoolean(parentPath);
    }
  }

  if (parentPath.isSequenceExpression()) {
    const {
      expressions
    } = parentPath.node;

    if (expressions[expressions.length - 1] === node) {
      return willPathCastToBoolean(parentPath);
    } else {
      return true;
    }
  }

  return parentPath.isConditional({
    test: node
  }) || parentPath.isUnaryExpression({
    operator: "!"
  }) || parentPath.isLoop({
    test: node
  });
}

class AssignmentMemoiser {
  constructor() {
    this._map = void 0;
    this._map = new WeakMap();
  }

  has(key) {
    return this._map.has(key);
  }

  get(key) {
    if (!this.has(key)) return;

    const record = this._map.get(key);

    const {
      value
    } = record;
    record.count--;

    if (record.count === 0) {
      return t__namespace.assignmentExpression("=", value, key);
    }

    return value;
  }

  set(key, value, count) {
    return this._map.set(key, {
      count,
      value
    });
  }

}

function toNonOptional(path, base) {
  const {
    node
  } = path;

  if (path.isOptionalMemberExpression()) {
    return t__namespace.memberExpression(base, node.property, node.computed);
  }

  if (path.isOptionalCallExpression()) {
    const callee = path.get("callee");

    if (path.node.optional && callee.isOptionalMemberExpression()) {
      const {
        object
      } = callee.node;
      const context = path.scope.maybeGenerateMemoised(object) || object;
      callee.get("object").replaceWith(t__namespace.assignmentExpression("=", context, object));
      return t__namespace.callExpression(t__namespace.memberExpression(base, t__namespace.identifier("call")), [context, ...node.arguments]);
    }

    return t__namespace.callExpression(base, node.arguments);
  }

  return path.node;
}

function isInDetachedTree(path) {
  while (path) {
    if (path.isProgram()) break;
    const {
      parentPath,
      container,
      listKey
    } = path;
    const parentNode = parentPath.node;

    if (listKey) {
      if (container !== parentNode[listKey]) return true;
    } else {
      if (container !== parentNode) return true;
    }

    path = parentPath;
  }

  return false;
}

const handle = {
  memoise() {},

  handle(member, noDocumentAll) {
    const {
      node,
      parent,
      parentPath,
      scope
    } = member;

    if (member.isOptionalMemberExpression()) {
      if (isInDetachedTree(member)) return;
      const endPath = member.find(({
        node,
        parent,
        parentPath
      }) => {
        if (parentPath.isOptionalMemberExpression()) {
          return parent.optional || parent.object !== node;
        }

        if (parentPath.isOptionalCallExpression()) {
          return node !== member.node && parent.optional || parent.callee !== node;
        }

        return true;
      });

      if (scope.path.isPattern()) {
        endPath.replaceWith(t__namespace.callExpression(t__namespace.arrowFunctionExpression([], endPath.node), []));
        return;
      }

      const willEndPathCastToBoolean = willPathCastToBoolean(endPath);
      const rootParentPath = endPath.parentPath;

      if (rootParentPath.isUpdateExpression({
        argument: node
      }) || rootParentPath.isAssignmentExpression({
        left: node
      })) {
        throw member.buildCodeFrameError(`can't handle assignment`);
      }

      const isDeleteOperation = rootParentPath.isUnaryExpression({
        operator: "delete"
      });

      if (isDeleteOperation && endPath.isOptionalMemberExpression() && endPath.get("property").isPrivateName()) {
        throw member.buildCodeFrameError(`can't delete a private class element`);
      }

      let startingOptional = member;

      for (;;) {
        if (startingOptional.isOptionalMemberExpression()) {
          if (startingOptional.node.optional) break;
          startingOptional = startingOptional.get("object");
          continue;
        } else if (startingOptional.isOptionalCallExpression()) {
          if (startingOptional.node.optional) break;
          startingOptional = startingOptional.get("callee");
          continue;
        }

        throw new Error(`Internal error: unexpected ${startingOptional.node.type}`);
      }

      const startingProp = startingOptional.isOptionalMemberExpression() ? "object" : "callee";
      const startingNode = startingOptional.node[startingProp];
      const baseNeedsMemoised = scope.maybeGenerateMemoised(startingNode);
      const baseRef = baseNeedsMemoised != null ? baseNeedsMemoised : startingNode;
      const parentIsOptionalCall = parentPath.isOptionalCallExpression({
        callee: node
      });
      const parentIsCall = parentPath.isCallExpression({
        callee: node
      });
      startingOptional.replaceWith(toNonOptional(startingOptional, baseRef));

      if (parentIsOptionalCall) {
        if (parent.optional) {
          parentPath.replaceWith(this.optionalCall(member, parent.arguments));
        } else {
          parentPath.replaceWith(this.call(member, parent.arguments));
        }
      } else if (parentIsCall) {
        member.replaceWith(this.boundGet(member));
      } else {
        member.replaceWith(this.get(member));
      }

      let regular = member.node;

      for (let current = member; current !== endPath;) {
        const {
          parentPath
        } = current;

        if (parentPath === endPath && parentIsOptionalCall && parent.optional) {
          regular = parentPath.node;
          break;
        }

        regular = toNonOptional(parentPath, regular);
        current = parentPath;
      }

      let context;
      const endParentPath = endPath.parentPath;

      if (t__namespace.isMemberExpression(regular) && endParentPath.isOptionalCallExpression({
        callee: endPath.node,
        optional: true
      })) {
        const {
          object
        } = regular;
        context = member.scope.maybeGenerateMemoised(object);

        if (context) {
          regular.object = t__namespace.assignmentExpression("=", context, object);
        }
      }

      let replacementPath = endPath;

      if (isDeleteOperation) {
        replacementPath = endParentPath;
        regular = endParentPath.node;
      }

      const baseMemoised = baseNeedsMemoised ? t__namespace.assignmentExpression("=", t__namespace.cloneNode(baseRef), t__namespace.cloneNode(startingNode)) : t__namespace.cloneNode(baseRef);

      if (willEndPathCastToBoolean) {
        let nonNullishCheck;

        if (noDocumentAll) {
          nonNullishCheck = t__namespace.binaryExpression("!=", baseMemoised, t__namespace.nullLiteral());
        } else {
          nonNullishCheck = t__namespace.logicalExpression("&&", t__namespace.binaryExpression("!==", baseMemoised, t__namespace.nullLiteral()), t__namespace.binaryExpression("!==", t__namespace.cloneNode(baseRef), scope.buildUndefinedNode()));
        }

        replacementPath.replaceWith(t__namespace.logicalExpression("&&", nonNullishCheck, regular));
      } else {
        let nullishCheck;

        if (noDocumentAll) {
          nullishCheck = t__namespace.binaryExpression("==", baseMemoised, t__namespace.nullLiteral());
        } else {
          nullishCheck = t__namespace.logicalExpression("||", t__namespace.binaryExpression("===", baseMemoised, t__namespace.nullLiteral()), t__namespace.binaryExpression("===", t__namespace.cloneNode(baseRef), scope.buildUndefinedNode()));
        }

        replacementPath.replaceWith(t__namespace.conditionalExpression(nullishCheck, isDeleteOperation ? t__namespace.booleanLiteral(true) : scope.buildUndefinedNode(), regular));
      }

      if (context) {
        const endParent = endParentPath.node;
        endParentPath.replaceWith(t__namespace.optionalCallExpression(t__namespace.optionalMemberExpression(endParent.callee, t__namespace.identifier("call"), false, true), [t__namespace.cloneNode(context), ...endParent.arguments], false));
      }

      return;
    }

    if (parentPath.isUpdateExpression({
      argument: node
    })) {
      if (this.simpleSet) {
        member.replaceWith(this.simpleSet(member));
        return;
      }

      const {
        operator,
        prefix
      } = parent;
      this.memoise(member, 2);
      const value = t__namespace.binaryExpression(operator[0], t__namespace.unaryExpression("+", this.get(member)), t__namespace.numericLiteral(1));

      if (prefix) {
        parentPath.replaceWith(this.set(member, value));
      } else {
        const {
          scope
        } = member;
        const ref = scope.generateUidIdentifierBasedOnNode(node);
        scope.push({
          id: ref
        });
        value.left = t__namespace.assignmentExpression("=", t__namespace.cloneNode(ref), value.left);
        parentPath.replaceWith(t__namespace.sequenceExpression([this.set(member, value), t__namespace.cloneNode(ref)]));
      }

      return;
    }

    if (parentPath.isAssignmentExpression({
      left: node
    })) {
      if (this.simpleSet) {
        member.replaceWith(this.simpleSet(member));
        return;
      }

      const {
        operator,
        right: value
      } = parent;

      if (operator === "=") {
        parentPath.replaceWith(this.set(member, value));
      } else {
        const operatorTrunc = operator.slice(0, -1);

        if (t__namespace.LOGICAL_OPERATORS.includes(operatorTrunc)) {
          this.memoise(member, 1);
          parentPath.replaceWith(t__namespace.logicalExpression(operatorTrunc, this.get(member), this.set(member, value)));
        } else {
          this.memoise(member, 2);
          parentPath.replaceWith(this.set(member, t__namespace.binaryExpression(operatorTrunc, this.get(member), value)));
        }
      }

      return;
    }

    if (parentPath.isCallExpression({
      callee: node
    })) {
      parentPath.replaceWith(this.call(member, parent.arguments));
      return;
    }

    if (parentPath.isOptionalCallExpression({
      callee: node
    })) {
      if (scope.path.isPattern()) {
        parentPath.replaceWith(t__namespace.callExpression(t__namespace.arrowFunctionExpression([], parentPath.node), []));
        return;
      }

      parentPath.replaceWith(this.optionalCall(member, parent.arguments));
      return;
    }

    if (parentPath.isForXStatement({
      left: node
    }) || parentPath.isObjectProperty({
      value: node
    }) && parentPath.parentPath.isObjectPattern() || parentPath.isAssignmentPattern({
      left: node
    }) && parentPath.parentPath.isObjectProperty({
      value: parent
    }) && parentPath.parentPath.parentPath.isObjectPattern() || parentPath.isArrayPattern() || parentPath.isAssignmentPattern({
      left: node
    }) && parentPath.parentPath.isArrayPattern() || parentPath.isRestElement()) {
      member.replaceWith(this.destructureSet(member));
      return;
    }

    member.replaceWith(this.get(member));
  }

};
function memberExpressionToFunctions(path, visitor, state) {
  path.traverse(visitor, Object.assign({}, handle, state, {
    memoiser: new AssignmentMemoiser()
  }));
}

exports.default = memberExpressionToFunctions;
//# sourceMappingURL=index.js.map
