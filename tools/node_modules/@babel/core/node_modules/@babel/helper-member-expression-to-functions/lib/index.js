'use strict';

Object.defineProperty(exports, '__esModule', { value: true });

var t = require('@babel/types');

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
      return t.assignmentExpression("=", value, key);
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
    return t.memberExpression(base, node.property, node.computed);
  }

  if (path.isOptionalCallExpression()) {
    const callee = path.get("callee");

    if (path.node.optional && callee.isOptionalMemberExpression()) {
      const {
        object
      } = callee.node;
      const context = path.scope.maybeGenerateMemoised(object) || object;
      callee.get("object").replaceWith(t.assignmentExpression("=", context, object));
      return t.callExpression(t.memberExpression(base, t.identifier("call")), [context, ...node.arguments]);
    }

    return t.callExpression(base, node.arguments);
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

  handle(member) {
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
        endPath.replaceWith(t.callExpression(t.arrowFunctionExpression([], endPath.node), []));
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

      if (t.isMemberExpression(regular) && endParentPath.isOptionalCallExpression({
        callee: endPath.node,
        optional: true
      })) {
        const {
          object
        } = regular;
        context = member.scope.maybeGenerateMemoised(object);

        if (context) {
          regular.object = t.assignmentExpression("=", context, object);
        }
      }

      let replacementPath = endPath;

      if (isDeleteOperation) {
        replacementPath = endParentPath;
        regular = endParentPath.node;
      }

      if (willEndPathCastToBoolean) {
        const nonNullishCheck = t.logicalExpression("&&", t.binaryExpression("!==", baseNeedsMemoised ? t.assignmentExpression("=", t.cloneNode(baseRef), t.cloneNode(startingNode)) : t.cloneNode(baseRef), t.nullLiteral()), t.binaryExpression("!==", t.cloneNode(baseRef), scope.buildUndefinedNode()));
        replacementPath.replaceWith(t.logicalExpression("&&", nonNullishCheck, regular));
      } else {
        const nullishCheck = t.logicalExpression("||", t.binaryExpression("===", baseNeedsMemoised ? t.assignmentExpression("=", t.cloneNode(baseRef), t.cloneNode(startingNode)) : t.cloneNode(baseRef), t.nullLiteral()), t.binaryExpression("===", t.cloneNode(baseRef), scope.buildUndefinedNode()));
        replacementPath.replaceWith(t.conditionalExpression(nullishCheck, isDeleteOperation ? t.booleanLiteral(true) : scope.buildUndefinedNode(), regular));
      }

      if (context) {
        const endParent = endParentPath.node;
        endParentPath.replaceWith(t.optionalCallExpression(t.optionalMemberExpression(endParent.callee, t.identifier("call"), false, true), [t.cloneNode(context), ...endParent.arguments], false));
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
      const value = t.binaryExpression(operator[0], t.unaryExpression("+", this.get(member)), t.numericLiteral(1));

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
        value.left = t.assignmentExpression("=", t.cloneNode(ref), value.left);
        parentPath.replaceWith(t.sequenceExpression([this.set(member, value), t.cloneNode(ref)]));
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

        if (t.LOGICAL_OPERATORS.includes(operatorTrunc)) {
          this.memoise(member, 1);
          parentPath.replaceWith(t.logicalExpression(operatorTrunc, this.get(member), this.set(member, value)));
        } else {
          this.memoise(member, 2);
          parentPath.replaceWith(this.set(member, t.binaryExpression(operatorTrunc, this.get(member), value)));
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
        parentPath.replaceWith(t.callExpression(t.arrowFunctionExpression([], parentPath.node), []));
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
