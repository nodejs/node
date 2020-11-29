"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.NullableTypeAnnotation = NullableTypeAnnotation;
exports.FunctionTypeAnnotation = FunctionTypeAnnotation;
exports.UpdateExpression = UpdateExpression;
exports.ObjectExpression = ObjectExpression;
exports.DoExpression = DoExpression;
exports.Binary = Binary;
exports.IntersectionTypeAnnotation = exports.UnionTypeAnnotation = UnionTypeAnnotation;
exports.TSAsExpression = TSAsExpression;
exports.TSTypeAssertion = TSTypeAssertion;
exports.TSIntersectionType = exports.TSUnionType = TSUnionType;
exports.TSInferType = TSInferType;
exports.BinaryExpression = BinaryExpression;
exports.SequenceExpression = SequenceExpression;
exports.AwaitExpression = exports.YieldExpression = YieldExpression;
exports.ClassExpression = ClassExpression;
exports.UnaryLike = UnaryLike;
exports.FunctionExpression = FunctionExpression;
exports.ArrowFunctionExpression = ArrowFunctionExpression;
exports.ConditionalExpression = ConditionalExpression;
exports.OptionalCallExpression = exports.OptionalMemberExpression = OptionalMemberExpression;
exports.AssignmentExpression = AssignmentExpression;
exports.LogicalExpression = LogicalExpression;

var t = _interopRequireWildcard(require("@babel/types"));

function _getRequireWildcardCache() { if (typeof WeakMap !== "function") return null; var cache = new WeakMap(); _getRequireWildcardCache = function () { return cache; }; return cache; }

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } if (obj === null || typeof obj !== "object" && typeof obj !== "function") { return { default: obj }; } var cache = _getRequireWildcardCache(); if (cache && cache.has(obj)) { return cache.get(obj); } var newObj = {}; var hasPropertyDescriptor = Object.defineProperty && Object.getOwnPropertyDescriptor; for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = hasPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : null; if (desc && (desc.get || desc.set)) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } newObj.default = obj; if (cache) { cache.set(obj, newObj); } return newObj; }

const PRECEDENCE = {
  "||": 0,
  "??": 0,
  "&&": 1,
  "|": 2,
  "^": 3,
  "&": 4,
  "==": 5,
  "===": 5,
  "!=": 5,
  "!==": 5,
  "<": 6,
  ">": 6,
  "<=": 6,
  ">=": 6,
  in: 6,
  instanceof: 6,
  ">>": 7,
  "<<": 7,
  ">>>": 7,
  "+": 8,
  "-": 8,
  "*": 9,
  "/": 9,
  "%": 9,
  "**": 10
};

const isClassExtendsClause = (node, parent) => (t.isClassDeclaration(parent) || t.isClassExpression(parent)) && parent.superClass === node;

const hasPostfixPart = (node, parent) => (t.isMemberExpression(parent) || t.isOptionalMemberExpression(parent)) && parent.object === node || (t.isCallExpression(parent) || t.isOptionalCallExpression(parent) || t.isNewExpression(parent)) && parent.callee === node || t.isTaggedTemplateExpression(parent) && parent.tag === node || t.isTSNonNullExpression(parent);

function NullableTypeAnnotation(node, parent) {
  return t.isArrayTypeAnnotation(parent);
}

function FunctionTypeAnnotation(node, parent, printStack) {
  return t.isUnionTypeAnnotation(parent) || t.isIntersectionTypeAnnotation(parent) || t.isArrayTypeAnnotation(parent) || t.isTypeAnnotation(parent) && t.isArrowFunctionExpression(printStack[printStack.length - 3]);
}

function UpdateExpression(node, parent) {
  return hasPostfixPart(node, parent) || isClassExtendsClause(node, parent);
}

function ObjectExpression(node, parent, printStack) {
  return isFirstInStatement(printStack, {
    considerArrow: true
  });
}

function DoExpression(node, parent, printStack) {
  return isFirstInStatement(printStack);
}

function Binary(node, parent) {
  if (node.operator === "**" && t.isBinaryExpression(parent, {
    operator: "**"
  })) {
    return parent.left === node;
  }

  if (isClassExtendsClause(node, parent)) {
    return true;
  }

  if (hasPostfixPart(node, parent) || t.isUnaryLike(parent) || t.isAwaitExpression(parent)) {
    return true;
  }

  if (t.isBinary(parent)) {
    const parentOp = parent.operator;
    const parentPos = PRECEDENCE[parentOp];
    const nodeOp = node.operator;
    const nodePos = PRECEDENCE[nodeOp];

    if (parentPos === nodePos && parent.right === node && !t.isLogicalExpression(parent) || parentPos > nodePos) {
      return true;
    }
  }
}

function UnionTypeAnnotation(node, parent) {
  return t.isArrayTypeAnnotation(parent) || t.isNullableTypeAnnotation(parent) || t.isIntersectionTypeAnnotation(parent) || t.isUnionTypeAnnotation(parent);
}

function TSAsExpression() {
  return true;
}

function TSTypeAssertion() {
  return true;
}

function TSUnionType(node, parent) {
  return t.isTSArrayType(parent) || t.isTSOptionalType(parent) || t.isTSIntersectionType(parent) || t.isTSUnionType(parent) || t.isTSRestType(parent);
}

function TSInferType(node, parent) {
  return t.isTSArrayType(parent) || t.isTSOptionalType(parent);
}

function BinaryExpression(node, parent) {
  return node.operator === "in" && (t.isVariableDeclarator(parent) || t.isFor(parent));
}

function SequenceExpression(node, parent) {
  if (t.isForStatement(parent) || t.isThrowStatement(parent) || t.isReturnStatement(parent) || t.isIfStatement(parent) && parent.test === node || t.isWhileStatement(parent) && parent.test === node || t.isForInStatement(parent) && parent.right === node || t.isSwitchStatement(parent) && parent.discriminant === node || t.isExpressionStatement(parent) && parent.expression === node) {
    return false;
  }

  return true;
}

function YieldExpression(node, parent) {
  return t.isBinary(parent) || t.isUnaryLike(parent) || hasPostfixPart(node, parent) || t.isAwaitExpression(parent) && t.isYieldExpression(node) || t.isConditionalExpression(parent) && node === parent.test || isClassExtendsClause(node, parent);
}

function ClassExpression(node, parent, printStack) {
  return isFirstInStatement(printStack, {
    considerDefaultExports: true
  });
}

function UnaryLike(node, parent) {
  return hasPostfixPart(node, parent) || t.isBinaryExpression(parent, {
    operator: "**",
    left: node
  }) || isClassExtendsClause(node, parent);
}

function FunctionExpression(node, parent, printStack) {
  return isFirstInStatement(printStack, {
    considerDefaultExports: true
  });
}

function ArrowFunctionExpression(node, parent) {
  return t.isExportDeclaration(parent) || ConditionalExpression(node, parent);
}

function ConditionalExpression(node, parent) {
  if (t.isUnaryLike(parent) || t.isBinary(parent) || t.isConditionalExpression(parent, {
    test: node
  }) || t.isAwaitExpression(parent) || t.isTSTypeAssertion(parent) || t.isTSAsExpression(parent)) {
    return true;
  }

  return UnaryLike(node, parent);
}

function OptionalMemberExpression(node, parent) {
  return t.isCallExpression(parent, {
    callee: node
  }) || t.isMemberExpression(parent, {
    object: node
  });
}

function AssignmentExpression(node, parent, printStack) {
  if (t.isObjectPattern(node.left)) {
    return true;
  } else {
    return ConditionalExpression(node, parent, printStack);
  }
}

function LogicalExpression(node, parent) {
  switch (node.operator) {
    case "||":
      if (!t.isLogicalExpression(parent)) return false;
      return parent.operator === "??" || parent.operator === "&&";

    case "&&":
      return t.isLogicalExpression(parent, {
        operator: "??"
      });

    case "??":
      return t.isLogicalExpression(parent) && parent.operator !== "??";
  }
}

function isFirstInStatement(printStack, {
  considerArrow = false,
  considerDefaultExports = false
} = {}) {
  let i = printStack.length - 1;
  let node = printStack[i];
  i--;
  let parent = printStack[i];

  while (i >= 0) {
    if (t.isExpressionStatement(parent, {
      expression: node
    }) || considerDefaultExports && t.isExportDefaultDeclaration(parent, {
      declaration: node
    }) || considerArrow && t.isArrowFunctionExpression(parent, {
      body: node
    })) {
      return true;
    }

    if (hasPostfixPart(node, parent) && !t.isNewExpression(parent) || t.isSequenceExpression(parent) && parent.expressions[0] === node || t.isConditional(parent, {
      test: node
    }) || t.isBinary(parent, {
      left: node
    }) || t.isAssignmentExpression(parent, {
      left: node
    })) {
      node = parent;
      i--;
      parent = printStack[i];
    } else {
      return false;
    }
  }

  return false;
}