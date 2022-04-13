"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.ArrayExpression = ArrayExpression;
exports.AssignmentExpression = AssignmentExpression;
exports.BinaryExpression = BinaryExpression;
exports.BooleanLiteral = BooleanLiteral;
exports.CallExpression = CallExpression;
exports.ConditionalExpression = ConditionalExpression;
exports.ClassDeclaration = exports.ClassExpression = exports.FunctionDeclaration = exports.ArrowFunctionExpression = exports.FunctionExpression = Func;
Object.defineProperty(exports, "Identifier", {
  enumerable: true,
  get: function () {
    return _infererReference.default;
  }
});
exports.LogicalExpression = LogicalExpression;
exports.NewExpression = NewExpression;
exports.NullLiteral = NullLiteral;
exports.NumericLiteral = NumericLiteral;
exports.ObjectExpression = ObjectExpression;
exports.ParenthesizedExpression = ParenthesizedExpression;
exports.RegExpLiteral = RegExpLiteral;
exports.RestElement = RestElement;
exports.SequenceExpression = SequenceExpression;
exports.StringLiteral = StringLiteral;
exports.TaggedTemplateExpression = TaggedTemplateExpression;
exports.TemplateLiteral = TemplateLiteral;
exports.TypeCastExpression = TypeCastExpression;
exports.UnaryExpression = UnaryExpression;
exports.UpdateExpression = UpdateExpression;
exports.VariableDeclarator = VariableDeclarator;

var _t = require("@babel/types");

var _infererReference = require("./inferer-reference");

const {
  BOOLEAN_BINARY_OPERATORS,
  BOOLEAN_UNARY_OPERATORS,
  NUMBER_BINARY_OPERATORS,
  NUMBER_UNARY_OPERATORS,
  STRING_UNARY_OPERATORS,
  anyTypeAnnotation,
  arrayTypeAnnotation,
  booleanTypeAnnotation,
  buildMatchMemberExpression,
  createFlowUnionType,
  createTSUnionType,
  createUnionTypeAnnotation,
  genericTypeAnnotation,
  identifier,
  isTSTypeAnnotation,
  nullLiteralTypeAnnotation,
  numberTypeAnnotation,
  stringTypeAnnotation,
  tupleTypeAnnotation,
  unionTypeAnnotation,
  voidTypeAnnotation
} = _t;

function VariableDeclarator() {
  var _type;

  const id = this.get("id");
  if (!id.isIdentifier()) return;
  const init = this.get("init");
  let type = init.getTypeAnnotation();

  if (((_type = type) == null ? void 0 : _type.type) === "AnyTypeAnnotation") {
    if (init.isCallExpression() && init.get("callee").isIdentifier({
      name: "Array"
    }) && !init.scope.hasBinding("Array", true)) {
      type = ArrayExpression();
    }
  }

  return type;
}

function TypeCastExpression(node) {
  return node.typeAnnotation;
}

TypeCastExpression.validParent = true;

function NewExpression(node) {
  if (this.get("callee").isIdentifier()) {
    return genericTypeAnnotation(node.callee);
  }
}

function TemplateLiteral() {
  return stringTypeAnnotation();
}

function UnaryExpression(node) {
  const operator = node.operator;

  if (operator === "void") {
    return voidTypeAnnotation();
  } else if (NUMBER_UNARY_OPERATORS.indexOf(operator) >= 0) {
    return numberTypeAnnotation();
  } else if (STRING_UNARY_OPERATORS.indexOf(operator) >= 0) {
    return stringTypeAnnotation();
  } else if (BOOLEAN_UNARY_OPERATORS.indexOf(operator) >= 0) {
    return booleanTypeAnnotation();
  }
}

function BinaryExpression(node) {
  const operator = node.operator;

  if (NUMBER_BINARY_OPERATORS.indexOf(operator) >= 0) {
    return numberTypeAnnotation();
  } else if (BOOLEAN_BINARY_OPERATORS.indexOf(operator) >= 0) {
    return booleanTypeAnnotation();
  } else if (operator === "+") {
    const right = this.get("right");
    const left = this.get("left");

    if (left.isBaseType("number") && right.isBaseType("number")) {
      return numberTypeAnnotation();
    } else if (left.isBaseType("string") || right.isBaseType("string")) {
      return stringTypeAnnotation();
    }

    return unionTypeAnnotation([stringTypeAnnotation(), numberTypeAnnotation()]);
  }
}

function LogicalExpression() {
  const argumentTypes = [this.get("left").getTypeAnnotation(), this.get("right").getTypeAnnotation()];

  if (isTSTypeAnnotation(argumentTypes[0]) && createTSUnionType) {
    return createTSUnionType(argumentTypes);
  }

  if (createFlowUnionType) {
    return createFlowUnionType(argumentTypes);
  }

  return createUnionTypeAnnotation(argumentTypes);
}

function ConditionalExpression() {
  const argumentTypes = [this.get("consequent").getTypeAnnotation(), this.get("alternate").getTypeAnnotation()];

  if (isTSTypeAnnotation(argumentTypes[0]) && createTSUnionType) {
    return createTSUnionType(argumentTypes);
  }

  if (createFlowUnionType) {
    return createFlowUnionType(argumentTypes);
  }

  return createUnionTypeAnnotation(argumentTypes);
}

function SequenceExpression() {
  return this.get("expressions").pop().getTypeAnnotation();
}

function ParenthesizedExpression() {
  return this.get("expression").getTypeAnnotation();
}

function AssignmentExpression() {
  return this.get("right").getTypeAnnotation();
}

function UpdateExpression(node) {
  const operator = node.operator;

  if (operator === "++" || operator === "--") {
    return numberTypeAnnotation();
  }
}

function StringLiteral() {
  return stringTypeAnnotation();
}

function NumericLiteral() {
  return numberTypeAnnotation();
}

function BooleanLiteral() {
  return booleanTypeAnnotation();
}

function NullLiteral() {
  return nullLiteralTypeAnnotation();
}

function RegExpLiteral() {
  return genericTypeAnnotation(identifier("RegExp"));
}

function ObjectExpression() {
  return genericTypeAnnotation(identifier("Object"));
}

function ArrayExpression() {
  return genericTypeAnnotation(identifier("Array"));
}

function RestElement() {
  return ArrayExpression();
}

RestElement.validParent = true;

function Func() {
  return genericTypeAnnotation(identifier("Function"));
}

const isArrayFrom = buildMatchMemberExpression("Array.from");
const isObjectKeys = buildMatchMemberExpression("Object.keys");
const isObjectValues = buildMatchMemberExpression("Object.values");
const isObjectEntries = buildMatchMemberExpression("Object.entries");

function CallExpression() {
  const {
    callee
  } = this.node;

  if (isObjectKeys(callee)) {
    return arrayTypeAnnotation(stringTypeAnnotation());
  } else if (isArrayFrom(callee) || isObjectValues(callee)) {
    return arrayTypeAnnotation(anyTypeAnnotation());
  } else if (isObjectEntries(callee)) {
    return arrayTypeAnnotation(tupleTypeAnnotation([stringTypeAnnotation(), anyTypeAnnotation()]));
  }

  return resolveCall(this.get("callee"));
}

function TaggedTemplateExpression() {
  return resolveCall(this.get("tag"));
}

function resolveCall(callee) {
  callee = callee.resolve();

  if (callee.isFunction()) {
    if (callee.is("async")) {
      if (callee.is("generator")) {
        return genericTypeAnnotation(identifier("AsyncIterator"));
      } else {
        return genericTypeAnnotation(identifier("Promise"));
      }
    } else {
      if (callee.node.returnType) {
        return callee.node.returnType;
      } else {}
    }
  }
}