"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.anyTypeAnnotation = anyTypeAnnotation;
exports.argumentPlaceholder = argumentPlaceholder;
exports.arrayExpression = arrayExpression;
exports.arrayPattern = arrayPattern;
exports.arrayTypeAnnotation = arrayTypeAnnotation;
exports.arrowFunctionExpression = arrowFunctionExpression;
exports.assignmentExpression = assignmentExpression;
exports.assignmentPattern = assignmentPattern;
exports.awaitExpression = awaitExpression;
exports.bigIntLiteral = bigIntLiteral;
exports.binaryExpression = binaryExpression;
exports.bindExpression = bindExpression;
exports.blockStatement = blockStatement;
exports.booleanLiteral = booleanLiteral;
exports.booleanLiteralTypeAnnotation = booleanLiteralTypeAnnotation;
exports.booleanTypeAnnotation = booleanTypeAnnotation;
exports.breakStatement = breakStatement;
exports.callExpression = callExpression;
exports.catchClause = catchClause;
exports.classAccessorProperty = classAccessorProperty;
exports.classBody = classBody;
exports.classDeclaration = classDeclaration;
exports.classExpression = classExpression;
exports.classImplements = classImplements;
exports.classMethod = classMethod;
exports.classPrivateMethod = classPrivateMethod;
exports.classPrivateProperty = classPrivateProperty;
exports.classProperty = classProperty;
exports.conditionalExpression = conditionalExpression;
exports.continueStatement = continueStatement;
exports.debuggerStatement = debuggerStatement;
exports.decimalLiteral = decimalLiteral;
exports.declareClass = declareClass;
exports.declareExportAllDeclaration = declareExportAllDeclaration;
exports.declareExportDeclaration = declareExportDeclaration;
exports.declareFunction = declareFunction;
exports.declareInterface = declareInterface;
exports.declareModule = declareModule;
exports.declareModuleExports = declareModuleExports;
exports.declareOpaqueType = declareOpaqueType;
exports.declareTypeAlias = declareTypeAlias;
exports.declareVariable = declareVariable;
exports.declaredPredicate = declaredPredicate;
exports.decorator = decorator;
exports.directive = directive;
exports.directiveLiteral = directiveLiteral;
exports.doExpression = doExpression;
exports.doWhileStatement = doWhileStatement;
exports.emptyStatement = emptyStatement;
exports.emptyTypeAnnotation = emptyTypeAnnotation;
exports.enumBooleanBody = enumBooleanBody;
exports.enumBooleanMember = enumBooleanMember;
exports.enumDeclaration = enumDeclaration;
exports.enumDefaultedMember = enumDefaultedMember;
exports.enumNumberBody = enumNumberBody;
exports.enumNumberMember = enumNumberMember;
exports.enumStringBody = enumStringBody;
exports.enumStringMember = enumStringMember;
exports.enumSymbolBody = enumSymbolBody;
exports.existsTypeAnnotation = existsTypeAnnotation;
exports.exportAllDeclaration = exportAllDeclaration;
exports.exportDefaultDeclaration = exportDefaultDeclaration;
exports.exportDefaultSpecifier = exportDefaultSpecifier;
exports.exportNamedDeclaration = exportNamedDeclaration;
exports.exportNamespaceSpecifier = exportNamespaceSpecifier;
exports.exportSpecifier = exportSpecifier;
exports.expressionStatement = expressionStatement;
exports.file = file;
exports.forInStatement = forInStatement;
exports.forOfStatement = forOfStatement;
exports.forStatement = forStatement;
exports.functionDeclaration = functionDeclaration;
exports.functionExpression = functionExpression;
exports.functionTypeAnnotation = functionTypeAnnotation;
exports.functionTypeParam = functionTypeParam;
exports.genericTypeAnnotation = genericTypeAnnotation;
exports.identifier = identifier;
exports.ifStatement = ifStatement;
exports.import = _import;
exports.importAttribute = importAttribute;
exports.importDeclaration = importDeclaration;
exports.importDefaultSpecifier = importDefaultSpecifier;
exports.importNamespaceSpecifier = importNamespaceSpecifier;
exports.importSpecifier = importSpecifier;
exports.indexedAccessType = indexedAccessType;
exports.inferredPredicate = inferredPredicate;
exports.interfaceDeclaration = interfaceDeclaration;
exports.interfaceExtends = interfaceExtends;
exports.interfaceTypeAnnotation = interfaceTypeAnnotation;
exports.interpreterDirective = interpreterDirective;
exports.intersectionTypeAnnotation = intersectionTypeAnnotation;
exports.jSXAttribute = exports.jsxAttribute = jsxAttribute;
exports.jSXClosingElement = exports.jsxClosingElement = jsxClosingElement;
exports.jSXClosingFragment = exports.jsxClosingFragment = jsxClosingFragment;
exports.jSXElement = exports.jsxElement = jsxElement;
exports.jSXEmptyExpression = exports.jsxEmptyExpression = jsxEmptyExpression;
exports.jSXExpressionContainer = exports.jsxExpressionContainer = jsxExpressionContainer;
exports.jSXFragment = exports.jsxFragment = jsxFragment;
exports.jSXIdentifier = exports.jsxIdentifier = jsxIdentifier;
exports.jSXMemberExpression = exports.jsxMemberExpression = jsxMemberExpression;
exports.jSXNamespacedName = exports.jsxNamespacedName = jsxNamespacedName;
exports.jSXOpeningElement = exports.jsxOpeningElement = jsxOpeningElement;
exports.jSXOpeningFragment = exports.jsxOpeningFragment = jsxOpeningFragment;
exports.jSXSpreadAttribute = exports.jsxSpreadAttribute = jsxSpreadAttribute;
exports.jSXSpreadChild = exports.jsxSpreadChild = jsxSpreadChild;
exports.jSXText = exports.jsxText = jsxText;
exports.labeledStatement = labeledStatement;
exports.logicalExpression = logicalExpression;
exports.memberExpression = memberExpression;
exports.metaProperty = metaProperty;
exports.mixedTypeAnnotation = mixedTypeAnnotation;
exports.moduleExpression = moduleExpression;
exports.newExpression = newExpression;
exports.noop = noop;
exports.nullLiteral = nullLiteral;
exports.nullLiteralTypeAnnotation = nullLiteralTypeAnnotation;
exports.nullableTypeAnnotation = nullableTypeAnnotation;
exports.numberLiteral = NumberLiteral;
exports.numberLiteralTypeAnnotation = numberLiteralTypeAnnotation;
exports.numberTypeAnnotation = numberTypeAnnotation;
exports.numericLiteral = numericLiteral;
exports.objectExpression = objectExpression;
exports.objectMethod = objectMethod;
exports.objectPattern = objectPattern;
exports.objectProperty = objectProperty;
exports.objectTypeAnnotation = objectTypeAnnotation;
exports.objectTypeCallProperty = objectTypeCallProperty;
exports.objectTypeIndexer = objectTypeIndexer;
exports.objectTypeInternalSlot = objectTypeInternalSlot;
exports.objectTypeProperty = objectTypeProperty;
exports.objectTypeSpreadProperty = objectTypeSpreadProperty;
exports.opaqueType = opaqueType;
exports.optionalCallExpression = optionalCallExpression;
exports.optionalIndexedAccessType = optionalIndexedAccessType;
exports.optionalMemberExpression = optionalMemberExpression;
exports.parenthesizedExpression = parenthesizedExpression;
exports.pipelineBareFunction = pipelineBareFunction;
exports.pipelinePrimaryTopicReference = pipelinePrimaryTopicReference;
exports.pipelineTopicExpression = pipelineTopicExpression;
exports.placeholder = placeholder;
exports.privateName = privateName;
exports.program = program;
exports.qualifiedTypeIdentifier = qualifiedTypeIdentifier;
exports.recordExpression = recordExpression;
exports.regExpLiteral = regExpLiteral;
exports.regexLiteral = RegexLiteral;
exports.restElement = restElement;
exports.restProperty = RestProperty;
exports.returnStatement = returnStatement;
exports.sequenceExpression = sequenceExpression;
exports.spreadElement = spreadElement;
exports.spreadProperty = SpreadProperty;
exports.staticBlock = staticBlock;
exports.stringLiteral = stringLiteral;
exports.stringLiteralTypeAnnotation = stringLiteralTypeAnnotation;
exports.stringTypeAnnotation = stringTypeAnnotation;
exports.super = _super;
exports.switchCase = switchCase;
exports.switchStatement = switchStatement;
exports.symbolTypeAnnotation = symbolTypeAnnotation;
exports.taggedTemplateExpression = taggedTemplateExpression;
exports.templateElement = templateElement;
exports.templateLiteral = templateLiteral;
exports.thisExpression = thisExpression;
exports.thisTypeAnnotation = thisTypeAnnotation;
exports.throwStatement = throwStatement;
exports.topicReference = topicReference;
exports.tryStatement = tryStatement;
exports.tSAnyKeyword = exports.tsAnyKeyword = tsAnyKeyword;
exports.tSArrayType = exports.tsArrayType = tsArrayType;
exports.tSAsExpression = exports.tsAsExpression = tsAsExpression;
exports.tSBigIntKeyword = exports.tsBigIntKeyword = tsBigIntKeyword;
exports.tSBooleanKeyword = exports.tsBooleanKeyword = tsBooleanKeyword;
exports.tSCallSignatureDeclaration = exports.tsCallSignatureDeclaration = tsCallSignatureDeclaration;
exports.tSConditionalType = exports.tsConditionalType = tsConditionalType;
exports.tSConstructSignatureDeclaration = exports.tsConstructSignatureDeclaration = tsConstructSignatureDeclaration;
exports.tSConstructorType = exports.tsConstructorType = tsConstructorType;
exports.tSDeclareFunction = exports.tsDeclareFunction = tsDeclareFunction;
exports.tSDeclareMethod = exports.tsDeclareMethod = tsDeclareMethod;
exports.tSEnumDeclaration = exports.tsEnumDeclaration = tsEnumDeclaration;
exports.tSEnumMember = exports.tsEnumMember = tsEnumMember;
exports.tSExportAssignment = exports.tsExportAssignment = tsExportAssignment;
exports.tSExpressionWithTypeArguments = exports.tsExpressionWithTypeArguments = tsExpressionWithTypeArguments;
exports.tSExternalModuleReference = exports.tsExternalModuleReference = tsExternalModuleReference;
exports.tSFunctionType = exports.tsFunctionType = tsFunctionType;
exports.tSImportEqualsDeclaration = exports.tsImportEqualsDeclaration = tsImportEqualsDeclaration;
exports.tSImportType = exports.tsImportType = tsImportType;
exports.tSIndexSignature = exports.tsIndexSignature = tsIndexSignature;
exports.tSIndexedAccessType = exports.tsIndexedAccessType = tsIndexedAccessType;
exports.tSInferType = exports.tsInferType = tsInferType;
exports.tSInterfaceBody = exports.tsInterfaceBody = tsInterfaceBody;
exports.tSInterfaceDeclaration = exports.tsInterfaceDeclaration = tsInterfaceDeclaration;
exports.tSIntersectionType = exports.tsIntersectionType = tsIntersectionType;
exports.tSIntrinsicKeyword = exports.tsIntrinsicKeyword = tsIntrinsicKeyword;
exports.tSLiteralType = exports.tsLiteralType = tsLiteralType;
exports.tSMappedType = exports.tsMappedType = tsMappedType;
exports.tSMethodSignature = exports.tsMethodSignature = tsMethodSignature;
exports.tSModuleBlock = exports.tsModuleBlock = tsModuleBlock;
exports.tSModuleDeclaration = exports.tsModuleDeclaration = tsModuleDeclaration;
exports.tSNamedTupleMember = exports.tsNamedTupleMember = tsNamedTupleMember;
exports.tSNamespaceExportDeclaration = exports.tsNamespaceExportDeclaration = tsNamespaceExportDeclaration;
exports.tSNeverKeyword = exports.tsNeverKeyword = tsNeverKeyword;
exports.tSNonNullExpression = exports.tsNonNullExpression = tsNonNullExpression;
exports.tSNullKeyword = exports.tsNullKeyword = tsNullKeyword;
exports.tSNumberKeyword = exports.tsNumberKeyword = tsNumberKeyword;
exports.tSObjectKeyword = exports.tsObjectKeyword = tsObjectKeyword;
exports.tSOptionalType = exports.tsOptionalType = tsOptionalType;
exports.tSParameterProperty = exports.tsParameterProperty = tsParameterProperty;
exports.tSParenthesizedType = exports.tsParenthesizedType = tsParenthesizedType;
exports.tSPropertySignature = exports.tsPropertySignature = tsPropertySignature;
exports.tSQualifiedName = exports.tsQualifiedName = tsQualifiedName;
exports.tSRestType = exports.tsRestType = tsRestType;
exports.tSStringKeyword = exports.tsStringKeyword = tsStringKeyword;
exports.tSSymbolKeyword = exports.tsSymbolKeyword = tsSymbolKeyword;
exports.tSThisType = exports.tsThisType = tsThisType;
exports.tSTupleType = exports.tsTupleType = tsTupleType;
exports.tSTypeAliasDeclaration = exports.tsTypeAliasDeclaration = tsTypeAliasDeclaration;
exports.tSTypeAnnotation = exports.tsTypeAnnotation = tsTypeAnnotation;
exports.tSTypeAssertion = exports.tsTypeAssertion = tsTypeAssertion;
exports.tSTypeLiteral = exports.tsTypeLiteral = tsTypeLiteral;
exports.tSTypeOperator = exports.tsTypeOperator = tsTypeOperator;
exports.tSTypeParameter = exports.tsTypeParameter = tsTypeParameter;
exports.tSTypeParameterDeclaration = exports.tsTypeParameterDeclaration = tsTypeParameterDeclaration;
exports.tSTypeParameterInstantiation = exports.tsTypeParameterInstantiation = tsTypeParameterInstantiation;
exports.tSTypePredicate = exports.tsTypePredicate = tsTypePredicate;
exports.tSTypeQuery = exports.tsTypeQuery = tsTypeQuery;
exports.tSTypeReference = exports.tsTypeReference = tsTypeReference;
exports.tSUndefinedKeyword = exports.tsUndefinedKeyword = tsUndefinedKeyword;
exports.tSUnionType = exports.tsUnionType = tsUnionType;
exports.tSUnknownKeyword = exports.tsUnknownKeyword = tsUnknownKeyword;
exports.tSVoidKeyword = exports.tsVoidKeyword = tsVoidKeyword;
exports.tupleExpression = tupleExpression;
exports.tupleTypeAnnotation = tupleTypeAnnotation;
exports.typeAlias = typeAlias;
exports.typeAnnotation = typeAnnotation;
exports.typeCastExpression = typeCastExpression;
exports.typeParameter = typeParameter;
exports.typeParameterDeclaration = typeParameterDeclaration;
exports.typeParameterInstantiation = typeParameterInstantiation;
exports.typeofTypeAnnotation = typeofTypeAnnotation;
exports.unaryExpression = unaryExpression;
exports.unionTypeAnnotation = unionTypeAnnotation;
exports.updateExpression = updateExpression;
exports.v8IntrinsicIdentifier = v8IntrinsicIdentifier;
exports.variableDeclaration = variableDeclaration;
exports.variableDeclarator = variableDeclarator;
exports.variance = variance;
exports.voidTypeAnnotation = voidTypeAnnotation;
exports.whileStatement = whileStatement;
exports.withStatement = withStatement;
exports.yieldExpression = yieldExpression;

var _builder = require("../builder");

function arrayExpression(elements) {
  return _builder.default.apply("ArrayExpression", arguments);
}

function assignmentExpression(operator, left, right) {
  return _builder.default.apply("AssignmentExpression", arguments);
}

function binaryExpression(operator, left, right) {
  return _builder.default.apply("BinaryExpression", arguments);
}

function interpreterDirective(value) {
  return _builder.default.apply("InterpreterDirective", arguments);
}

function directive(value) {
  return _builder.default.apply("Directive", arguments);
}

function directiveLiteral(value) {
  return _builder.default.apply("DirectiveLiteral", arguments);
}

function blockStatement(body, directives) {
  return _builder.default.apply("BlockStatement", arguments);
}

function breakStatement(label) {
  return _builder.default.apply("BreakStatement", arguments);
}

function callExpression(callee, _arguments) {
  return _builder.default.apply("CallExpression", arguments);
}

function catchClause(param, body) {
  return _builder.default.apply("CatchClause", arguments);
}

function conditionalExpression(test, consequent, alternate) {
  return _builder.default.apply("ConditionalExpression", arguments);
}

function continueStatement(label) {
  return _builder.default.apply("ContinueStatement", arguments);
}

function debuggerStatement() {
  return _builder.default.apply("DebuggerStatement", arguments);
}

function doWhileStatement(test, body) {
  return _builder.default.apply("DoWhileStatement", arguments);
}

function emptyStatement() {
  return _builder.default.apply("EmptyStatement", arguments);
}

function expressionStatement(expression) {
  return _builder.default.apply("ExpressionStatement", arguments);
}

function file(program, comments, tokens) {
  return _builder.default.apply("File", arguments);
}

function forInStatement(left, right, body) {
  return _builder.default.apply("ForInStatement", arguments);
}

function forStatement(init, test, update, body) {
  return _builder.default.apply("ForStatement", arguments);
}

function functionDeclaration(id, params, body, generator, async) {
  return _builder.default.apply("FunctionDeclaration", arguments);
}

function functionExpression(id, params, body, generator, async) {
  return _builder.default.apply("FunctionExpression", arguments);
}

function identifier(name) {
  return _builder.default.apply("Identifier", arguments);
}

function ifStatement(test, consequent, alternate) {
  return _builder.default.apply("IfStatement", arguments);
}

function labeledStatement(label, body) {
  return _builder.default.apply("LabeledStatement", arguments);
}

function stringLiteral(value) {
  return _builder.default.apply("StringLiteral", arguments);
}

function numericLiteral(value) {
  return _builder.default.apply("NumericLiteral", arguments);
}

function nullLiteral() {
  return _builder.default.apply("NullLiteral", arguments);
}

function booleanLiteral(value) {
  return _builder.default.apply("BooleanLiteral", arguments);
}

function regExpLiteral(pattern, flags) {
  return _builder.default.apply("RegExpLiteral", arguments);
}

function logicalExpression(operator, left, right) {
  return _builder.default.apply("LogicalExpression", arguments);
}

function memberExpression(object, property, computed, optional) {
  return _builder.default.apply("MemberExpression", arguments);
}

function newExpression(callee, _arguments) {
  return _builder.default.apply("NewExpression", arguments);
}

function program(body, directives, sourceType, interpreter) {
  return _builder.default.apply("Program", arguments);
}

function objectExpression(properties) {
  return _builder.default.apply("ObjectExpression", arguments);
}

function objectMethod(kind, key, params, body, computed, generator, async) {
  return _builder.default.apply("ObjectMethod", arguments);
}

function objectProperty(key, value, computed, shorthand, decorators) {
  return _builder.default.apply("ObjectProperty", arguments);
}

function restElement(argument) {
  return _builder.default.apply("RestElement", arguments);
}

function returnStatement(argument) {
  return _builder.default.apply("ReturnStatement", arguments);
}

function sequenceExpression(expressions) {
  return _builder.default.apply("SequenceExpression", arguments);
}

function parenthesizedExpression(expression) {
  return _builder.default.apply("ParenthesizedExpression", arguments);
}

function switchCase(test, consequent) {
  return _builder.default.apply("SwitchCase", arguments);
}

function switchStatement(discriminant, cases) {
  return _builder.default.apply("SwitchStatement", arguments);
}

function thisExpression() {
  return _builder.default.apply("ThisExpression", arguments);
}

function throwStatement(argument) {
  return _builder.default.apply("ThrowStatement", arguments);
}

function tryStatement(block, handler, finalizer) {
  return _builder.default.apply("TryStatement", arguments);
}

function unaryExpression(operator, argument, prefix) {
  return _builder.default.apply("UnaryExpression", arguments);
}

function updateExpression(operator, argument, prefix) {
  return _builder.default.apply("UpdateExpression", arguments);
}

function variableDeclaration(kind, declarations) {
  return _builder.default.apply("VariableDeclaration", arguments);
}

function variableDeclarator(id, init) {
  return _builder.default.apply("VariableDeclarator", arguments);
}

function whileStatement(test, body) {
  return _builder.default.apply("WhileStatement", arguments);
}

function withStatement(object, body) {
  return _builder.default.apply("WithStatement", arguments);
}

function assignmentPattern(left, right) {
  return _builder.default.apply("AssignmentPattern", arguments);
}

function arrayPattern(elements) {
  return _builder.default.apply("ArrayPattern", arguments);
}

function arrowFunctionExpression(params, body, async) {
  return _builder.default.apply("ArrowFunctionExpression", arguments);
}

function classBody(body) {
  return _builder.default.apply("ClassBody", arguments);
}

function classExpression(id, superClass, body, decorators) {
  return _builder.default.apply("ClassExpression", arguments);
}

function classDeclaration(id, superClass, body, decorators) {
  return _builder.default.apply("ClassDeclaration", arguments);
}

function exportAllDeclaration(source) {
  return _builder.default.apply("ExportAllDeclaration", arguments);
}

function exportDefaultDeclaration(declaration) {
  return _builder.default.apply("ExportDefaultDeclaration", arguments);
}

function exportNamedDeclaration(declaration, specifiers, source) {
  return _builder.default.apply("ExportNamedDeclaration", arguments);
}

function exportSpecifier(local, exported) {
  return _builder.default.apply("ExportSpecifier", arguments);
}

function forOfStatement(left, right, body, _await) {
  return _builder.default.apply("ForOfStatement", arguments);
}

function importDeclaration(specifiers, source) {
  return _builder.default.apply("ImportDeclaration", arguments);
}

function importDefaultSpecifier(local) {
  return _builder.default.apply("ImportDefaultSpecifier", arguments);
}

function importNamespaceSpecifier(local) {
  return _builder.default.apply("ImportNamespaceSpecifier", arguments);
}

function importSpecifier(local, imported) {
  return _builder.default.apply("ImportSpecifier", arguments);
}

function metaProperty(meta, property) {
  return _builder.default.apply("MetaProperty", arguments);
}

function classMethod(kind, key, params, body, computed, _static, generator, async) {
  return _builder.default.apply("ClassMethod", arguments);
}

function objectPattern(properties) {
  return _builder.default.apply("ObjectPattern", arguments);
}

function spreadElement(argument) {
  return _builder.default.apply("SpreadElement", arguments);
}

function _super() {
  return _builder.default.apply("Super", arguments);
}

function taggedTemplateExpression(tag, quasi) {
  return _builder.default.apply("TaggedTemplateExpression", arguments);
}

function templateElement(value, tail) {
  return _builder.default.apply("TemplateElement", arguments);
}

function templateLiteral(quasis, expressions) {
  return _builder.default.apply("TemplateLiteral", arguments);
}

function yieldExpression(argument, delegate) {
  return _builder.default.apply("YieldExpression", arguments);
}

function awaitExpression(argument) {
  return _builder.default.apply("AwaitExpression", arguments);
}

function _import() {
  return _builder.default.apply("Import", arguments);
}

function bigIntLiteral(value) {
  return _builder.default.apply("BigIntLiteral", arguments);
}

function exportNamespaceSpecifier(exported) {
  return _builder.default.apply("ExportNamespaceSpecifier", arguments);
}

function optionalMemberExpression(object, property, computed, optional) {
  return _builder.default.apply("OptionalMemberExpression", arguments);
}

function optionalCallExpression(callee, _arguments, optional) {
  return _builder.default.apply("OptionalCallExpression", arguments);
}

function classProperty(key, value, typeAnnotation, decorators, computed, _static) {
  return _builder.default.apply("ClassProperty", arguments);
}

function classAccessorProperty(key, value, typeAnnotation, decorators, computed, _static) {
  return _builder.default.apply("ClassAccessorProperty", arguments);
}

function classPrivateProperty(key, value, decorators, _static) {
  return _builder.default.apply("ClassPrivateProperty", arguments);
}

function classPrivateMethod(kind, key, params, body, _static) {
  return _builder.default.apply("ClassPrivateMethod", arguments);
}

function privateName(id) {
  return _builder.default.apply("PrivateName", arguments);
}

function staticBlock(body) {
  return _builder.default.apply("StaticBlock", arguments);
}

function anyTypeAnnotation() {
  return _builder.default.apply("AnyTypeAnnotation", arguments);
}

function arrayTypeAnnotation(elementType) {
  return _builder.default.apply("ArrayTypeAnnotation", arguments);
}

function booleanTypeAnnotation() {
  return _builder.default.apply("BooleanTypeAnnotation", arguments);
}

function booleanLiteralTypeAnnotation(value) {
  return _builder.default.apply("BooleanLiteralTypeAnnotation", arguments);
}

function nullLiteralTypeAnnotation() {
  return _builder.default.apply("NullLiteralTypeAnnotation", arguments);
}

function classImplements(id, typeParameters) {
  return _builder.default.apply("ClassImplements", arguments);
}

function declareClass(id, typeParameters, _extends, body) {
  return _builder.default.apply("DeclareClass", arguments);
}

function declareFunction(id) {
  return _builder.default.apply("DeclareFunction", arguments);
}

function declareInterface(id, typeParameters, _extends, body) {
  return _builder.default.apply("DeclareInterface", arguments);
}

function declareModule(id, body, kind) {
  return _builder.default.apply("DeclareModule", arguments);
}

function declareModuleExports(typeAnnotation) {
  return _builder.default.apply("DeclareModuleExports", arguments);
}

function declareTypeAlias(id, typeParameters, right) {
  return _builder.default.apply("DeclareTypeAlias", arguments);
}

function declareOpaqueType(id, typeParameters, supertype) {
  return _builder.default.apply("DeclareOpaqueType", arguments);
}

function declareVariable(id) {
  return _builder.default.apply("DeclareVariable", arguments);
}

function declareExportDeclaration(declaration, specifiers, source) {
  return _builder.default.apply("DeclareExportDeclaration", arguments);
}

function declareExportAllDeclaration(source) {
  return _builder.default.apply("DeclareExportAllDeclaration", arguments);
}

function declaredPredicate(value) {
  return _builder.default.apply("DeclaredPredicate", arguments);
}

function existsTypeAnnotation() {
  return _builder.default.apply("ExistsTypeAnnotation", arguments);
}

function functionTypeAnnotation(typeParameters, params, rest, returnType) {
  return _builder.default.apply("FunctionTypeAnnotation", arguments);
}

function functionTypeParam(name, typeAnnotation) {
  return _builder.default.apply("FunctionTypeParam", arguments);
}

function genericTypeAnnotation(id, typeParameters) {
  return _builder.default.apply("GenericTypeAnnotation", arguments);
}

function inferredPredicate() {
  return _builder.default.apply("InferredPredicate", arguments);
}

function interfaceExtends(id, typeParameters) {
  return _builder.default.apply("InterfaceExtends", arguments);
}

function interfaceDeclaration(id, typeParameters, _extends, body) {
  return _builder.default.apply("InterfaceDeclaration", arguments);
}

function interfaceTypeAnnotation(_extends, body) {
  return _builder.default.apply("InterfaceTypeAnnotation", arguments);
}

function intersectionTypeAnnotation(types) {
  return _builder.default.apply("IntersectionTypeAnnotation", arguments);
}

function mixedTypeAnnotation() {
  return _builder.default.apply("MixedTypeAnnotation", arguments);
}

function emptyTypeAnnotation() {
  return _builder.default.apply("EmptyTypeAnnotation", arguments);
}

function nullableTypeAnnotation(typeAnnotation) {
  return _builder.default.apply("NullableTypeAnnotation", arguments);
}

function numberLiteralTypeAnnotation(value) {
  return _builder.default.apply("NumberLiteralTypeAnnotation", arguments);
}

function numberTypeAnnotation() {
  return _builder.default.apply("NumberTypeAnnotation", arguments);
}

function objectTypeAnnotation(properties, indexers, callProperties, internalSlots, exact) {
  return _builder.default.apply("ObjectTypeAnnotation", arguments);
}

function objectTypeInternalSlot(id, value, optional, _static, method) {
  return _builder.default.apply("ObjectTypeInternalSlot", arguments);
}

function objectTypeCallProperty(value) {
  return _builder.default.apply("ObjectTypeCallProperty", arguments);
}

function objectTypeIndexer(id, key, value, variance) {
  return _builder.default.apply("ObjectTypeIndexer", arguments);
}

function objectTypeProperty(key, value, variance) {
  return _builder.default.apply("ObjectTypeProperty", arguments);
}

function objectTypeSpreadProperty(argument) {
  return _builder.default.apply("ObjectTypeSpreadProperty", arguments);
}

function opaqueType(id, typeParameters, supertype, impltype) {
  return _builder.default.apply("OpaqueType", arguments);
}

function qualifiedTypeIdentifier(id, qualification) {
  return _builder.default.apply("QualifiedTypeIdentifier", arguments);
}

function stringLiteralTypeAnnotation(value) {
  return _builder.default.apply("StringLiteralTypeAnnotation", arguments);
}

function stringTypeAnnotation() {
  return _builder.default.apply("StringTypeAnnotation", arguments);
}

function symbolTypeAnnotation() {
  return _builder.default.apply("SymbolTypeAnnotation", arguments);
}

function thisTypeAnnotation() {
  return _builder.default.apply("ThisTypeAnnotation", arguments);
}

function tupleTypeAnnotation(types) {
  return _builder.default.apply("TupleTypeAnnotation", arguments);
}

function typeofTypeAnnotation(argument) {
  return _builder.default.apply("TypeofTypeAnnotation", arguments);
}

function typeAlias(id, typeParameters, right) {
  return _builder.default.apply("TypeAlias", arguments);
}

function typeAnnotation(typeAnnotation) {
  return _builder.default.apply("TypeAnnotation", arguments);
}

function typeCastExpression(expression, typeAnnotation) {
  return _builder.default.apply("TypeCastExpression", arguments);
}

function typeParameter(bound, _default, variance) {
  return _builder.default.apply("TypeParameter", arguments);
}

function typeParameterDeclaration(params) {
  return _builder.default.apply("TypeParameterDeclaration", arguments);
}

function typeParameterInstantiation(params) {
  return _builder.default.apply("TypeParameterInstantiation", arguments);
}

function unionTypeAnnotation(types) {
  return _builder.default.apply("UnionTypeAnnotation", arguments);
}

function variance(kind) {
  return _builder.default.apply("Variance", arguments);
}

function voidTypeAnnotation() {
  return _builder.default.apply("VoidTypeAnnotation", arguments);
}

function enumDeclaration(id, body) {
  return _builder.default.apply("EnumDeclaration", arguments);
}

function enumBooleanBody(members) {
  return _builder.default.apply("EnumBooleanBody", arguments);
}

function enumNumberBody(members) {
  return _builder.default.apply("EnumNumberBody", arguments);
}

function enumStringBody(members) {
  return _builder.default.apply("EnumStringBody", arguments);
}

function enumSymbolBody(members) {
  return _builder.default.apply("EnumSymbolBody", arguments);
}

function enumBooleanMember(id) {
  return _builder.default.apply("EnumBooleanMember", arguments);
}

function enumNumberMember(id, init) {
  return _builder.default.apply("EnumNumberMember", arguments);
}

function enumStringMember(id, init) {
  return _builder.default.apply("EnumStringMember", arguments);
}

function enumDefaultedMember(id) {
  return _builder.default.apply("EnumDefaultedMember", arguments);
}

function indexedAccessType(objectType, indexType) {
  return _builder.default.apply("IndexedAccessType", arguments);
}

function optionalIndexedAccessType(objectType, indexType) {
  return _builder.default.apply("OptionalIndexedAccessType", arguments);
}

function jsxAttribute(name, value) {
  return _builder.default.apply("JSXAttribute", arguments);
}

function jsxClosingElement(name) {
  return _builder.default.apply("JSXClosingElement", arguments);
}

function jsxElement(openingElement, closingElement, children, selfClosing) {
  return _builder.default.apply("JSXElement", arguments);
}

function jsxEmptyExpression() {
  return _builder.default.apply("JSXEmptyExpression", arguments);
}

function jsxExpressionContainer(expression) {
  return _builder.default.apply("JSXExpressionContainer", arguments);
}

function jsxSpreadChild(expression) {
  return _builder.default.apply("JSXSpreadChild", arguments);
}

function jsxIdentifier(name) {
  return _builder.default.apply("JSXIdentifier", arguments);
}

function jsxMemberExpression(object, property) {
  return _builder.default.apply("JSXMemberExpression", arguments);
}

function jsxNamespacedName(namespace, name) {
  return _builder.default.apply("JSXNamespacedName", arguments);
}

function jsxOpeningElement(name, attributes, selfClosing) {
  return _builder.default.apply("JSXOpeningElement", arguments);
}

function jsxSpreadAttribute(argument) {
  return _builder.default.apply("JSXSpreadAttribute", arguments);
}

function jsxText(value) {
  return _builder.default.apply("JSXText", arguments);
}

function jsxFragment(openingFragment, closingFragment, children) {
  return _builder.default.apply("JSXFragment", arguments);
}

function jsxOpeningFragment() {
  return _builder.default.apply("JSXOpeningFragment", arguments);
}

function jsxClosingFragment() {
  return _builder.default.apply("JSXClosingFragment", arguments);
}

function noop() {
  return _builder.default.apply("Noop", arguments);
}

function placeholder(expectedNode, name) {
  return _builder.default.apply("Placeholder", arguments);
}

function v8IntrinsicIdentifier(name) {
  return _builder.default.apply("V8IntrinsicIdentifier", arguments);
}

function argumentPlaceholder() {
  return _builder.default.apply("ArgumentPlaceholder", arguments);
}

function bindExpression(object, callee) {
  return _builder.default.apply("BindExpression", arguments);
}

function importAttribute(key, value) {
  return _builder.default.apply("ImportAttribute", arguments);
}

function decorator(expression) {
  return _builder.default.apply("Decorator", arguments);
}

function doExpression(body, async) {
  return _builder.default.apply("DoExpression", arguments);
}

function exportDefaultSpecifier(exported) {
  return _builder.default.apply("ExportDefaultSpecifier", arguments);
}

function recordExpression(properties) {
  return _builder.default.apply("RecordExpression", arguments);
}

function tupleExpression(elements) {
  return _builder.default.apply("TupleExpression", arguments);
}

function decimalLiteral(value) {
  return _builder.default.apply("DecimalLiteral", arguments);
}

function moduleExpression(body) {
  return _builder.default.apply("ModuleExpression", arguments);
}

function topicReference() {
  return _builder.default.apply("TopicReference", arguments);
}

function pipelineTopicExpression(expression) {
  return _builder.default.apply("PipelineTopicExpression", arguments);
}

function pipelineBareFunction(callee) {
  return _builder.default.apply("PipelineBareFunction", arguments);
}

function pipelinePrimaryTopicReference() {
  return _builder.default.apply("PipelinePrimaryTopicReference", arguments);
}

function tsParameterProperty(parameter) {
  return _builder.default.apply("TSParameterProperty", arguments);
}

function tsDeclareFunction(id, typeParameters, params, returnType) {
  return _builder.default.apply("TSDeclareFunction", arguments);
}

function tsDeclareMethod(decorators, key, typeParameters, params, returnType) {
  return _builder.default.apply("TSDeclareMethod", arguments);
}

function tsQualifiedName(left, right) {
  return _builder.default.apply("TSQualifiedName", arguments);
}

function tsCallSignatureDeclaration(typeParameters, parameters, typeAnnotation) {
  return _builder.default.apply("TSCallSignatureDeclaration", arguments);
}

function tsConstructSignatureDeclaration(typeParameters, parameters, typeAnnotation) {
  return _builder.default.apply("TSConstructSignatureDeclaration", arguments);
}

function tsPropertySignature(key, typeAnnotation, initializer) {
  return _builder.default.apply("TSPropertySignature", arguments);
}

function tsMethodSignature(key, typeParameters, parameters, typeAnnotation) {
  return _builder.default.apply("TSMethodSignature", arguments);
}

function tsIndexSignature(parameters, typeAnnotation) {
  return _builder.default.apply("TSIndexSignature", arguments);
}

function tsAnyKeyword() {
  return _builder.default.apply("TSAnyKeyword", arguments);
}

function tsBooleanKeyword() {
  return _builder.default.apply("TSBooleanKeyword", arguments);
}

function tsBigIntKeyword() {
  return _builder.default.apply("TSBigIntKeyword", arguments);
}

function tsIntrinsicKeyword() {
  return _builder.default.apply("TSIntrinsicKeyword", arguments);
}

function tsNeverKeyword() {
  return _builder.default.apply("TSNeverKeyword", arguments);
}

function tsNullKeyword() {
  return _builder.default.apply("TSNullKeyword", arguments);
}

function tsNumberKeyword() {
  return _builder.default.apply("TSNumberKeyword", arguments);
}

function tsObjectKeyword() {
  return _builder.default.apply("TSObjectKeyword", arguments);
}

function tsStringKeyword() {
  return _builder.default.apply("TSStringKeyword", arguments);
}

function tsSymbolKeyword() {
  return _builder.default.apply("TSSymbolKeyword", arguments);
}

function tsUndefinedKeyword() {
  return _builder.default.apply("TSUndefinedKeyword", arguments);
}

function tsUnknownKeyword() {
  return _builder.default.apply("TSUnknownKeyword", arguments);
}

function tsVoidKeyword() {
  return _builder.default.apply("TSVoidKeyword", arguments);
}

function tsThisType() {
  return _builder.default.apply("TSThisType", arguments);
}

function tsFunctionType(typeParameters, parameters, typeAnnotation) {
  return _builder.default.apply("TSFunctionType", arguments);
}

function tsConstructorType(typeParameters, parameters, typeAnnotation) {
  return _builder.default.apply("TSConstructorType", arguments);
}

function tsTypeReference(typeName, typeParameters) {
  return _builder.default.apply("TSTypeReference", arguments);
}

function tsTypePredicate(parameterName, typeAnnotation, asserts) {
  return _builder.default.apply("TSTypePredicate", arguments);
}

function tsTypeQuery(exprName) {
  return _builder.default.apply("TSTypeQuery", arguments);
}

function tsTypeLiteral(members) {
  return _builder.default.apply("TSTypeLiteral", arguments);
}

function tsArrayType(elementType) {
  return _builder.default.apply("TSArrayType", arguments);
}

function tsTupleType(elementTypes) {
  return _builder.default.apply("TSTupleType", arguments);
}

function tsOptionalType(typeAnnotation) {
  return _builder.default.apply("TSOptionalType", arguments);
}

function tsRestType(typeAnnotation) {
  return _builder.default.apply("TSRestType", arguments);
}

function tsNamedTupleMember(label, elementType, optional) {
  return _builder.default.apply("TSNamedTupleMember", arguments);
}

function tsUnionType(types) {
  return _builder.default.apply("TSUnionType", arguments);
}

function tsIntersectionType(types) {
  return _builder.default.apply("TSIntersectionType", arguments);
}

function tsConditionalType(checkType, extendsType, trueType, falseType) {
  return _builder.default.apply("TSConditionalType", arguments);
}

function tsInferType(typeParameter) {
  return _builder.default.apply("TSInferType", arguments);
}

function tsParenthesizedType(typeAnnotation) {
  return _builder.default.apply("TSParenthesizedType", arguments);
}

function tsTypeOperator(typeAnnotation) {
  return _builder.default.apply("TSTypeOperator", arguments);
}

function tsIndexedAccessType(objectType, indexType) {
  return _builder.default.apply("TSIndexedAccessType", arguments);
}

function tsMappedType(typeParameter, typeAnnotation, nameType) {
  return _builder.default.apply("TSMappedType", arguments);
}

function tsLiteralType(literal) {
  return _builder.default.apply("TSLiteralType", arguments);
}

function tsExpressionWithTypeArguments(expression, typeParameters) {
  return _builder.default.apply("TSExpressionWithTypeArguments", arguments);
}

function tsInterfaceDeclaration(id, typeParameters, _extends, body) {
  return _builder.default.apply("TSInterfaceDeclaration", arguments);
}

function tsInterfaceBody(body) {
  return _builder.default.apply("TSInterfaceBody", arguments);
}

function tsTypeAliasDeclaration(id, typeParameters, typeAnnotation) {
  return _builder.default.apply("TSTypeAliasDeclaration", arguments);
}

function tsAsExpression(expression, typeAnnotation) {
  return _builder.default.apply("TSAsExpression", arguments);
}

function tsTypeAssertion(typeAnnotation, expression) {
  return _builder.default.apply("TSTypeAssertion", arguments);
}

function tsEnumDeclaration(id, members) {
  return _builder.default.apply("TSEnumDeclaration", arguments);
}

function tsEnumMember(id, initializer) {
  return _builder.default.apply("TSEnumMember", arguments);
}

function tsModuleDeclaration(id, body) {
  return _builder.default.apply("TSModuleDeclaration", arguments);
}

function tsModuleBlock(body) {
  return _builder.default.apply("TSModuleBlock", arguments);
}

function tsImportType(argument, qualifier, typeParameters) {
  return _builder.default.apply("TSImportType", arguments);
}

function tsImportEqualsDeclaration(id, moduleReference) {
  return _builder.default.apply("TSImportEqualsDeclaration", arguments);
}

function tsExternalModuleReference(expression) {
  return _builder.default.apply("TSExternalModuleReference", arguments);
}

function tsNonNullExpression(expression) {
  return _builder.default.apply("TSNonNullExpression", arguments);
}

function tsExportAssignment(expression) {
  return _builder.default.apply("TSExportAssignment", arguments);
}

function tsNamespaceExportDeclaration(id) {
  return _builder.default.apply("TSNamespaceExportDeclaration", arguments);
}

function tsTypeAnnotation(typeAnnotation) {
  return _builder.default.apply("TSTypeAnnotation", arguments);
}

function tsTypeParameterInstantiation(params) {
  return _builder.default.apply("TSTypeParameterInstantiation", arguments);
}

function tsTypeParameterDeclaration(params) {
  return _builder.default.apply("TSTypeParameterDeclaration", arguments);
}

function tsTypeParameter(constraint, _default, name) {
  return _builder.default.apply("TSTypeParameter", arguments);
}

function NumberLiteral(value) {
  console.trace("The node type NumberLiteral has been renamed to NumericLiteral");
  return _builder.default.apply("NumberLiteral", arguments);
}

function RegexLiteral(pattern, flags) {
  console.trace("The node type RegexLiteral has been renamed to RegExpLiteral");
  return _builder.default.apply("RegexLiteral", arguments);
}

function RestProperty(argument) {
  console.trace("The node type RestProperty has been renamed to RestElement");
  return _builder.default.apply("RestProperty", arguments);
}

function SpreadProperty(argument) {
  console.trace("The node type SpreadProperty has been renamed to SpreadElement");
  return _builder.default.apply("SpreadProperty", arguments);
}