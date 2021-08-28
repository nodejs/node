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
exports.OptionalIndexedAccessType = OptionalIndexedAccessType;
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
exports.Identifier = Identifier;

var t = require("@babel/types");

const {
  isArrayTypeAnnotation,
  isArrowFunctionExpression,
  isAssignmentExpression,
  isAwaitExpression,
  isBinary,
  isBinaryExpression,
  isCallExpression,
  isClassDeclaration,
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
} = t;
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

const isClassExtendsClause = (node, parent) => (isClassDeclaration(parent) || isClassExpression(parent)) && parent.superClass === node;

const hasPostfixPart = (node, parent) => (isMemberExpression(parent) || isOptionalMemberExpression(parent)) && parent.object === node || (isCallExpression(parent) || isOptionalCallExpression(parent) || isNewExpression(parent)) && parent.callee === node || isTaggedTemplateExpression(parent) && parent.tag === node || isTSNonNullExpression(parent);

function NullableTypeAnnotation(node, parent) {
  return isArrayTypeAnnotation(parent);
}

function FunctionTypeAnnotation(node, parent, printStack) {
  return isUnionTypeAnnotation(parent) || isIntersectionTypeAnnotation(parent) || isArrayTypeAnnotation(parent) || isTypeAnnotation(parent) && isArrowFunctionExpression(printStack[printStack.length - 3]);
}

function UpdateExpression(node, parent) {
  return hasPostfixPart(node, parent) || isClassExtendsClause(node, parent);
}

function ObjectExpression(node, parent, printStack) {
  return isFirstInContext(printStack, {
    expressionStatement: true,
    arrowBody: true
  });
}

function DoExpression(node, parent, printStack) {
  return !node.async && isFirstInContext(printStack, {
    expressionStatement: true
  });
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
  return isFirstInContext(printStack, {
    expressionStatement: true,
    exportDefault: true
  });
}

function UnaryLike(node, parent) {
  return hasPostfixPart(node, parent) || isBinaryExpression(parent, {
    operator: "**",
    left: node
  }) || isClassExtendsClause(node, parent);
}

function FunctionExpression(node, parent, printStack) {
  return isFirstInContext(printStack, {
    expressionStatement: true,
    exportDefault: true
  });
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
  if (node.name === "let") {
    const isFollowedByBracket = isMemberExpression(parent, {
      object: node,
      computed: true
    }) || isOptionalMemberExpression(parent, {
      object: node,
      computed: true,
      optional: false
    });
    return isFirstInContext(printStack, {
      expressionStatement: isFollowedByBracket,
      forHead: isFollowedByBracket,
      forInHead: isFollowedByBracket,
      forOfHead: true
    });
  }

  return node.name === "async" && isForOfStatement(parent) && node === parent.left;
}

function isFirstInContext(printStack, {
  expressionStatement = false,
  arrowBody = false,
  exportDefault = false,
  forHead = false,
  forInHead = false,
  forOfHead = false
}) {
  let i = printStack.length - 1;
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

    if (hasPostfixPart(node, parent) && !isNewExpression(parent) || isSequenceExpression(parent) && parent.expressions[0] === node || isConditional(parent, {
      test: node
    }) || isBinary(parent, {
      left: node
    }) || isAssignmentExpression(parent, {
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