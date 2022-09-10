"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.ArrowFunctionExpression = ArrowFunctionExpression;
exports.AssignmentExpression = AssignmentExpression;
exports.Binary = Binary;
exports.BinaryExpression = BinaryExpression;
exports.ClassExpression = ClassExpression;
exports.ConditionalExpression = ConditionalExpression;
exports.DoExpression = DoExpression;
exports.FunctionExpression = FunctionExpression;
exports.FunctionTypeAnnotation = FunctionTypeAnnotation;
exports.Identifier = Identifier;
exports.LogicalExpression = LogicalExpression;
exports.NullableTypeAnnotation = NullableTypeAnnotation;
exports.ObjectExpression = ObjectExpression;
exports.OptionalIndexedAccessType = OptionalIndexedAccessType;
exports.OptionalCallExpression = exports.OptionalMemberExpression = OptionalMemberExpression;
exports.SequenceExpression = SequenceExpression;
exports.TSAsExpression = TSAsExpression;
exports.TSInferType = TSInferType;
exports.TSInstantiationExpression = TSInstantiationExpression;
exports.TSTypeAssertion = TSTypeAssertion;
exports.TSIntersectionType = exports.TSUnionType = TSUnionType;
exports.UnaryLike = UnaryLike;
exports.IntersectionTypeAnnotation = exports.UnionTypeAnnotation = UnionTypeAnnotation;
exports.UpdateExpression = UpdateExpression;
exports.AwaitExpression = exports.YieldExpression = YieldExpression;

var _t = require("@babel/types");

const {
  isArrayTypeAnnotation,
  isArrowFunctionExpression,
  isAssignmentExpression,
  isAwaitExpression,
  isBinary,
  isBinaryExpression,
  isUpdateExpression,
  isCallExpression,
  isClass,
  isClassExpression,
  isConditional,
  isConditionalExpression,
  isExportDeclaration,
  isExportDefaultDeclaration,
  isExpressionStatement,
  isFor,
  isForInStatement,
  isForOfStatement,
  isForStatement,
  isFunctionExpression,
  isIfStatement,
  isIndexedAccessType,
  isIntersectionTypeAnnotation,
  isLogicalExpression,
  isMemberExpression,
  isNewExpression,
  isNullableTypeAnnotation,
  isObjectPattern,
  isOptionalCallExpression,
  isOptionalMemberExpression,
  isReturnStatement,
  isSequenceExpression,
  isSwitchStatement,
  isTSArrayType,
  isTSAsExpression,
  isTSInstantiationExpression,
  isTSIntersectionType,
  isTSNonNullExpression,
  isTSOptionalType,
  isTSRestType,
  isTSTypeAssertion,
  isTSUnionType,
  isTaggedTemplateExpression,
  isThrowStatement,
  isTypeAnnotation,
  isUnaryLike,
  isUnionTypeAnnotation,
  isVariableDeclarator,
  isWhileStatement,
  isYieldExpression
} = _t;
const PRECEDENCE = {
  "||": 0,
  "??": 0,
  "|>": 0,
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

const isClassExtendsClause = (node, parent) => isClass(parent, {
  superClass: node
});

const hasPostfixPart = (node, parent) => (isMemberExpression(parent) || isOptionalMemberExpression(parent)) && parent.object === node || (isCallExpression(parent) || isOptionalCallExpression(parent) || isNewExpression(parent)) && parent.callee === node || isTaggedTemplateExpression(parent) && parent.tag === node || isTSNonNullExpression(parent);

function NullableTypeAnnotation(node, parent) {
  return isArrayTypeAnnotation(parent);
}

function FunctionTypeAnnotation(node, parent, printStack) {
  if (printStack.length < 3) return;
  return isUnionTypeAnnotation(parent) || isIntersectionTypeAnnotation(parent) || isArrayTypeAnnotation(parent) || isTypeAnnotation(parent) && isArrowFunctionExpression(printStack[printStack.length - 3]);
}

function UpdateExpression(node, parent) {
  return hasPostfixPart(node, parent) || isClassExtendsClause(node, parent);
}

function ObjectExpression(node, parent, printStack) {
  return isFirstInContext(printStack, 1 | 2);
}

function DoExpression(node, parent, printStack) {
  return !node.async && isFirstInContext(printStack, 1);
}

function Binary(node, parent) {
  if (node.operator === "**" && isBinaryExpression(parent, {
    operator: "**"
  })) {
    return parent.left === node;
  }

  if (isClassExtendsClause(node, parent)) {
    return true;
  }

  if (hasPostfixPart(node, parent) || isUnaryLike(parent) || isAwaitExpression(parent)) {
    return true;
  }

  if (isBinary(parent)) {
    const parentOp = parent.operator;
    const parentPos = PRECEDENCE[parentOp];
    const nodeOp = node.operator;
    const nodePos = PRECEDENCE[nodeOp];

    if (parentPos === nodePos && parent.right === node && !isLogicalExpression(parent) || parentPos > nodePos) {
      return true;
    }
  }
}

function UnionTypeAnnotation(node, parent) {
  return isArrayTypeAnnotation(parent) || isNullableTypeAnnotation(parent) || isIntersectionTypeAnnotation(parent) || isUnionTypeAnnotation(parent);
}

function OptionalIndexedAccessType(node, parent) {
  return isIndexedAccessType(parent, {
    objectType: node
  });
}

function TSAsExpression() {
  return true;
}

function TSTypeAssertion() {
  return true;
}

function TSUnionType(node, parent) {
  return isTSArrayType(parent) || isTSOptionalType(parent) || isTSIntersectionType(parent) || isTSUnionType(parent) || isTSRestType(parent);
}

function TSInferType(node, parent) {
  return isTSArrayType(parent) || isTSOptionalType(parent);
}

function TSInstantiationExpression(node, parent) {
  return (isCallExpression(parent) || isOptionalCallExpression(parent) || isNewExpression(parent) || isTSInstantiationExpression(parent)) && !!parent.typeParameters;
}

function BinaryExpression(node, parent) {
  return node.operator === "in" && (isVariableDeclarator(parent) || isFor(parent));
}

function SequenceExpression(node, parent) {
  if (isForStatement(parent) || isThrowStatement(parent) || isReturnStatement(parent) || isIfStatement(parent) && parent.test === node || isWhileStatement(parent) && parent.test === node || isForInStatement(parent) && parent.right === node || isSwitchStatement(parent) && parent.discriminant === node || isExpressionStatement(parent) && parent.expression === node) {
    return false;
  }

  return true;
}

function YieldExpression(node, parent) {
  return isBinary(parent) || isUnaryLike(parent) || hasPostfixPart(node, parent) || isAwaitExpression(parent) && isYieldExpression(node) || isConditionalExpression(parent) && node === parent.test || isClassExtendsClause(node, parent);
}

function ClassExpression(node, parent, printStack) {
  return isFirstInContext(printStack, 1 | 4);
}

function UnaryLike(node, parent) {
  return hasPostfixPart(node, parent) || isBinaryExpression(parent, {
    operator: "**",
    left: node
  }) || isClassExtendsClause(node, parent);
}

function FunctionExpression(node, parent, printStack) {
  return isFirstInContext(printStack, 1 | 4);
}

function ArrowFunctionExpression(node, parent) {
  return isExportDeclaration(parent) || ConditionalExpression(node, parent);
}

function ConditionalExpression(node, parent) {
  if (isUnaryLike(parent) || isBinary(parent) || isConditionalExpression(parent, {
    test: node
  }) || isAwaitExpression(parent) || isTSTypeAssertion(parent) || isTSAsExpression(parent)) {
    return true;
  }

  return UnaryLike(node, parent);
}

function OptionalMemberExpression(node, parent) {
  return isCallExpression(parent, {
    callee: node
  }) || isMemberExpression(parent, {
    object: node
  });
}

function AssignmentExpression(node, parent) {
  if (isObjectPattern(node.left)) {
    return true;
  } else {
    return ConditionalExpression(node, parent);
  }
}

function LogicalExpression(node, parent) {
  switch (node.operator) {
    case "||":
      if (!isLogicalExpression(parent)) return false;
      return parent.operator === "??" || parent.operator === "&&";

    case "&&":
      return isLogicalExpression(parent, {
        operator: "??"
      });

    case "??":
      return isLogicalExpression(parent) && parent.operator !== "??";
  }
}

function Identifier(node, parent, printStack) {
  var _node$extra;

  if ((_node$extra = node.extra) != null && _node$extra.parenthesized && isAssignmentExpression(parent, {
    left: node
  }) && (isFunctionExpression(parent.right) || isClassExpression(parent.right)) && parent.right.id == null) {
    return true;
  }

  if (node.name === "let") {
    const isFollowedByBracket = isMemberExpression(parent, {
      object: node,
      computed: true
    }) || isOptionalMemberExpression(parent, {
      object: node,
      computed: true,
      optional: false
    });
    return isFirstInContext(printStack, isFollowedByBracket ? 1 | 8 | 16 | 32 : 32);
  }

  return node.name === "async" && isForOfStatement(parent) && node === parent.left;
}

function isFirstInContext(printStack, checkParam) {
  const expressionStatement = checkParam & 1;
  const arrowBody = checkParam & 2;
  const exportDefault = checkParam & 4;
  const forHead = checkParam & 8;
  const forInHead = checkParam & 16;
  const forOfHead = checkParam & 32;
  let i = printStack.length - 1;
  if (i <= 0) return;
  let node = printStack[i];
  i--;
  let parent = printStack[i];

  while (i >= 0) {
    if (expressionStatement && isExpressionStatement(parent, {
      expression: node
    }) || exportDefault && isExportDefaultDeclaration(parent, {
      declaration: node
    }) || arrowBody && isArrowFunctionExpression(parent, {
      body: node
    }) || forHead && isForStatement(parent, {
      init: node
    }) || forInHead && isForInStatement(parent, {
      left: node
    }) || forOfHead && isForOfStatement(parent, {
      left: node
    })) {
      return true;
    }

    if (i > 0 && (hasPostfixPart(node, parent) && !isNewExpression(parent) || isSequenceExpression(parent) && parent.expressions[0] === node || isUpdateExpression(parent) && !parent.prefix || isConditional(parent, {
      test: node
    }) || isBinary(parent, {
      left: node
    }) || isAssignmentExpression(parent, {
      left: node
    }))) {
      node = parent;
      i--;
      parent = printStack[i];
    } else {
      return false;
    }
  }

  return false;
}