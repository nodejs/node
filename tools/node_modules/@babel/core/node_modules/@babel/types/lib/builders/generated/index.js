"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.ArrayExpression = exports.arrayExpression = arrayExpression;
exports.AssignmentExpression = exports.assignmentExpression = assignmentExpression;
exports.BinaryExpression = exports.binaryExpression = binaryExpression;
exports.InterpreterDirective = exports.interpreterDirective = interpreterDirective;
exports.Directive = exports.directive = directive;
exports.DirectiveLiteral = exports.directiveLiteral = directiveLiteral;
exports.BlockStatement = exports.blockStatement = blockStatement;
exports.BreakStatement = exports.breakStatement = breakStatement;
exports.CallExpression = exports.callExpression = callExpression;
exports.CatchClause = exports.catchClause = catchClause;
exports.ConditionalExpression = exports.conditionalExpression = conditionalExpression;
exports.ContinueStatement = exports.continueStatement = continueStatement;
exports.DebuggerStatement = exports.debuggerStatement = debuggerStatement;
exports.DoWhileStatement = exports.doWhileStatement = doWhileStatement;
exports.EmptyStatement = exports.emptyStatement = emptyStatement;
exports.ExpressionStatement = exports.expressionStatement = expressionStatement;
exports.File = exports.file = file;
exports.ForInStatement = exports.forInStatement = forInStatement;
exports.ForStatement = exports.forStatement = forStatement;
exports.FunctionDeclaration = exports.functionDeclaration = functionDeclaration;
exports.FunctionExpression = exports.functionExpression = functionExpression;
exports.Identifier = exports.identifier = identifier;
exports.IfStatement = exports.ifStatement = ifStatement;
exports.LabeledStatement = exports.labeledStatement = labeledStatement;
exports.StringLiteral = exports.stringLiteral = stringLiteral;
exports.NumericLiteral = exports.numericLiteral = numericLiteral;
exports.NullLiteral = exports.nullLiteral = nullLiteral;
exports.BooleanLiteral = exports.booleanLiteral = booleanLiteral;
exports.RegExpLiteral = exports.regExpLiteral = regExpLiteral;
exports.LogicalExpression = exports.logicalExpression = logicalExpression;
exports.MemberExpression = exports.memberExpression = memberExpression;
exports.NewExpression = exports.newExpression = newExpression;
exports.Program = exports.program = program;
exports.ObjectExpression = exports.objectExpression = objectExpression;
exports.ObjectMethod = exports.objectMethod = objectMethod;
exports.ObjectProperty = exports.objectProperty = objectProperty;
exports.RestElement = exports.restElement = restElement;
exports.ReturnStatement = exports.returnStatement = returnStatement;
exports.SequenceExpression = exports.sequenceExpression = sequenceExpression;
exports.ParenthesizedExpression = exports.parenthesizedExpression = parenthesizedExpression;
exports.SwitchCase = exports.switchCase = switchCase;
exports.SwitchStatement = exports.switchStatement = switchStatement;
exports.ThisExpression = exports.thisExpression = thisExpression;
exports.ThrowStatement = exports.throwStatement = throwStatement;
exports.TryStatement = exports.tryStatement = tryStatement;
exports.UnaryExpression = exports.unaryExpression = unaryExpression;
exports.UpdateExpression = exports.updateExpression = updateExpression;
exports.VariableDeclaration = exports.variableDeclaration = variableDeclaration;
exports.VariableDeclarator = exports.variableDeclarator = variableDeclarator;
exports.WhileStatement = exports.whileStatement = whileStatement;
exports.WithStatement = exports.withStatement = withStatement;
exports.AssignmentPattern = exports.assignmentPattern = assignmentPattern;
exports.ArrayPattern = exports.arrayPattern = arrayPattern;
exports.ArrowFunctionExpression = exports.arrowFunctionExpression = arrowFunctionExpression;
exports.ClassBody = exports.classBody = classBody;
exports.ClassExpression = exports.classExpression = classExpression;
exports.ClassDeclaration = exports.classDeclaration = classDeclaration;
exports.ExportAllDeclaration = exports.exportAllDeclaration = exportAllDeclaration;
exports.ExportDefaultDeclaration = exports.exportDefaultDeclaration = exportDefaultDeclaration;
exports.ExportNamedDeclaration = exports.exportNamedDeclaration = exportNamedDeclaration;
exports.ExportSpecifier = exports.exportSpecifier = exportSpecifier;
exports.ForOfStatement = exports.forOfStatement = forOfStatement;
exports.ImportDeclaration = exports.importDeclaration = importDeclaration;
exports.ImportDefaultSpecifier = exports.importDefaultSpecifier = importDefaultSpecifier;
exports.ImportNamespaceSpecifier = exports.importNamespaceSpecifier = importNamespaceSpecifier;
exports.ImportSpecifier = exports.importSpecifier = importSpecifier;
exports.MetaProperty = exports.metaProperty = metaProperty;
exports.ClassMethod = exports.classMethod = classMethod;
exports.ObjectPattern = exports.objectPattern = objectPattern;
exports.SpreadElement = exports.spreadElement = spreadElement;
exports.super = exports.Super = _super;
exports.TaggedTemplateExpression = exports.taggedTemplateExpression = taggedTemplateExpression;
exports.TemplateElement = exports.templateElement = templateElement;
exports.TemplateLiteral = exports.templateLiteral = templateLiteral;
exports.YieldExpression = exports.yieldExpression = yieldExpression;
exports.AwaitExpression = exports.awaitExpression = awaitExpression;
exports.import = exports.Import = _import;
exports.BigIntLiteral = exports.bigIntLiteral = bigIntLiteral;
exports.ExportNamespaceSpecifier = exports.exportNamespaceSpecifier = exportNamespaceSpecifier;
exports.OptionalMemberExpression = exports.optionalMemberExpression = optionalMemberExpression;
exports.OptionalCallExpression = exports.optionalCallExpression = optionalCallExpression;
exports.AnyTypeAnnotation = exports.anyTypeAnnotation = anyTypeAnnotation;
exports.ArrayTypeAnnotation = exports.arrayTypeAnnotation = arrayTypeAnnotation;
exports.BooleanTypeAnnotation = exports.booleanTypeAnnotation = booleanTypeAnnotation;
exports.BooleanLiteralTypeAnnotation = exports.booleanLiteralTypeAnnotation = booleanLiteralTypeAnnotation;
exports.NullLiteralTypeAnnotation = exports.nullLiteralTypeAnnotation = nullLiteralTypeAnnotation;
exports.ClassImplements = exports.classImplements = classImplements;
exports.DeclareClass = exports.declareClass = declareClass;
exports.DeclareFunction = exports.declareFunction = declareFunction;
exports.DeclareInterface = exports.declareInterface = declareInterface;
exports.DeclareModule = exports.declareModule = declareModule;
exports.DeclareModuleExports = exports.declareModuleExports = declareModuleExports;
exports.DeclareTypeAlias = exports.declareTypeAlias = declareTypeAlias;
exports.DeclareOpaqueType = exports.declareOpaqueType = declareOpaqueType;
exports.DeclareVariable = exports.declareVariable = declareVariable;
exports.DeclareExportDeclaration = exports.declareExportDeclaration = declareExportDeclaration;
exports.DeclareExportAllDeclaration = exports.declareExportAllDeclaration = declareExportAllDeclaration;
exports.DeclaredPredicate = exports.declaredPredicate = declaredPredicate;
exports.ExistsTypeAnnotation = exports.existsTypeAnnotation = existsTypeAnnotation;
exports.FunctionTypeAnnotation = exports.functionTypeAnnotation = functionTypeAnnotation;
exports.FunctionTypeParam = exports.functionTypeParam = functionTypeParam;
exports.GenericTypeAnnotation = exports.genericTypeAnnotation = genericTypeAnnotation;
exports.InferredPredicate = exports.inferredPredicate = inferredPredicate;
exports.InterfaceExtends = exports.interfaceExtends = interfaceExtends;
exports.InterfaceDeclaration = exports.interfaceDeclaration = interfaceDeclaration;
exports.InterfaceTypeAnnotation = exports.interfaceTypeAnnotation = interfaceTypeAnnotation;
exports.IntersectionTypeAnnotation = exports.intersectionTypeAnnotation = intersectionTypeAnnotation;
exports.MixedTypeAnnotation = exports.mixedTypeAnnotation = mixedTypeAnnotation;
exports.EmptyTypeAnnotation = exports.emptyTypeAnnotation = emptyTypeAnnotation;
exports.NullableTypeAnnotation = exports.nullableTypeAnnotation = nullableTypeAnnotation;
exports.NumberLiteralTypeAnnotation = exports.numberLiteralTypeAnnotation = numberLiteralTypeAnnotation;
exports.NumberTypeAnnotation = exports.numberTypeAnnotation = numberTypeAnnotation;
exports.ObjectTypeAnnotation = exports.objectTypeAnnotation = objectTypeAnnotation;
exports.ObjectTypeInternalSlot = exports.objectTypeInternalSlot = objectTypeInternalSlot;
exports.ObjectTypeCallProperty = exports.objectTypeCallProperty = objectTypeCallProperty;
exports.ObjectTypeIndexer = exports.objectTypeIndexer = objectTypeIndexer;
exports.ObjectTypeProperty = exports.objectTypeProperty = objectTypeProperty;
exports.ObjectTypeSpreadProperty = exports.objectTypeSpreadProperty = objectTypeSpreadProperty;
exports.OpaqueType = exports.opaqueType = opaqueType;
exports.QualifiedTypeIdentifier = exports.qualifiedTypeIdentifier = qualifiedTypeIdentifier;
exports.StringLiteralTypeAnnotation = exports.stringLiteralTypeAnnotation = stringLiteralTypeAnnotation;
exports.StringTypeAnnotation = exports.stringTypeAnnotation = stringTypeAnnotation;
exports.SymbolTypeAnnotation = exports.symbolTypeAnnotation = symbolTypeAnnotation;
exports.ThisTypeAnnotation = exports.thisTypeAnnotation = thisTypeAnnotation;
exports.TupleTypeAnnotation = exports.tupleTypeAnnotation = tupleTypeAnnotation;
exports.TypeofTypeAnnotation = exports.typeofTypeAnnotation = typeofTypeAnnotation;
exports.TypeAlias = exports.typeAlias = typeAlias;
exports.TypeAnnotation = exports.typeAnnotation = typeAnnotation;
exports.TypeCastExpression = exports.typeCastExpression = typeCastExpression;
exports.TypeParameter = exports.typeParameter = typeParameter;
exports.TypeParameterDeclaration = exports.typeParameterDeclaration = typeParameterDeclaration;
exports.TypeParameterInstantiation = exports.typeParameterInstantiation = typeParameterInstantiation;
exports.UnionTypeAnnotation = exports.unionTypeAnnotation = unionTypeAnnotation;
exports.Variance = exports.variance = variance;
exports.VoidTypeAnnotation = exports.voidTypeAnnotation = voidTypeAnnotation;
exports.EnumDeclaration = exports.enumDeclaration = enumDeclaration;
exports.EnumBooleanBody = exports.enumBooleanBody = enumBooleanBody;
exports.EnumNumberBody = exports.enumNumberBody = enumNumberBody;
exports.EnumStringBody = exports.enumStringBody = enumStringBody;
exports.EnumSymbolBody = exports.enumSymbolBody = enumSymbolBody;
exports.EnumBooleanMember = exports.enumBooleanMember = enumBooleanMember;
exports.EnumNumberMember = exports.enumNumberMember = enumNumberMember;
exports.EnumStringMember = exports.enumStringMember = enumStringMember;
exports.EnumDefaultedMember = exports.enumDefaultedMember = enumDefaultedMember;
exports.jSXAttribute = exports.JSXAttribute = exports.jsxAttribute = jsxAttribute;
exports.jSXClosingElement = exports.JSXClosingElement = exports.jsxClosingElement = jsxClosingElement;
exports.jSXElement = exports.JSXElement = exports.jsxElement = jsxElement;
exports.jSXEmptyExpression = exports.JSXEmptyExpression = exports.jsxEmptyExpression = jsxEmptyExpression;
exports.jSXExpressionContainer = exports.JSXExpressionContainer = exports.jsxExpressionContainer = jsxExpressionContainer;
exports.jSXSpreadChild = exports.JSXSpreadChild = exports.jsxSpreadChild = jsxSpreadChild;
exports.jSXIdentifier = exports.JSXIdentifier = exports.jsxIdentifier = jsxIdentifier;
exports.jSXMemberExpression = exports.JSXMemberExpression = exports.jsxMemberExpression = jsxMemberExpression;
exports.jSXNamespacedName = exports.JSXNamespacedName = exports.jsxNamespacedName = jsxNamespacedName;
exports.jSXOpeningElement = exports.JSXOpeningElement = exports.jsxOpeningElement = jsxOpeningElement;
exports.jSXSpreadAttribute = exports.JSXSpreadAttribute = exports.jsxSpreadAttribute = jsxSpreadAttribute;
exports.jSXText = exports.JSXText = exports.jsxText = jsxText;
exports.jSXFragment = exports.JSXFragment = exports.jsxFragment = jsxFragment;
exports.jSXOpeningFragment = exports.JSXOpeningFragment = exports.jsxOpeningFragment = jsxOpeningFragment;
exports.jSXClosingFragment = exports.JSXClosingFragment = exports.jsxClosingFragment = jsxClosingFragment;
exports.Noop = exports.noop = noop;
exports.Placeholder = exports.placeholder = placeholder;
exports.V8IntrinsicIdentifier = exports.v8IntrinsicIdentifier = v8IntrinsicIdentifier;
exports.ArgumentPlaceholder = exports.argumentPlaceholder = argumentPlaceholder;
exports.BindExpression = exports.bindExpression = bindExpression;
exports.ClassProperty = exports.classProperty = classProperty;
exports.PipelineTopicExpression = exports.pipelineTopicExpression = pipelineTopicExpression;
exports.PipelineBareFunction = exports.pipelineBareFunction = pipelineBareFunction;
exports.PipelinePrimaryTopicReference = exports.pipelinePrimaryTopicReference = pipelinePrimaryTopicReference;
exports.ClassPrivateProperty = exports.classPrivateProperty = classPrivateProperty;
exports.ClassPrivateMethod = exports.classPrivateMethod = classPrivateMethod;
exports.ImportAttribute = exports.importAttribute = importAttribute;
exports.Decorator = exports.decorator = decorator;
exports.DoExpression = exports.doExpression = doExpression;
exports.ExportDefaultSpecifier = exports.exportDefaultSpecifier = exportDefaultSpecifier;
exports.PrivateName = exports.privateName = privateName;
exports.RecordExpression = exports.recordExpression = recordExpression;
exports.TupleExpression = exports.tupleExpression = tupleExpression;
exports.DecimalLiteral = exports.decimalLiteral = decimalLiteral;
exports.StaticBlock = exports.staticBlock = staticBlock;
exports.tSParameterProperty = exports.TSParameterProperty = exports.tsParameterProperty = tsParameterProperty;
exports.tSDeclareFunction = exports.TSDeclareFunction = exports.tsDeclareFunction = tsDeclareFunction;
exports.tSDeclareMethod = exports.TSDeclareMethod = exports.tsDeclareMethod = tsDeclareMethod;
exports.tSQualifiedName = exports.TSQualifiedName = exports.tsQualifiedName = tsQualifiedName;
exports.tSCallSignatureDeclaration = exports.TSCallSignatureDeclaration = exports.tsCallSignatureDeclaration = tsCallSignatureDeclaration;
exports.tSConstructSignatureDeclaration = exports.TSConstructSignatureDeclaration = exports.tsConstructSignatureDeclaration = tsConstructSignatureDeclaration;
exports.tSPropertySignature = exports.TSPropertySignature = exports.tsPropertySignature = tsPropertySignature;
exports.tSMethodSignature = exports.TSMethodSignature = exports.tsMethodSignature = tsMethodSignature;
exports.tSIndexSignature = exports.TSIndexSignature = exports.tsIndexSignature = tsIndexSignature;
exports.tSAnyKeyword = exports.TSAnyKeyword = exports.tsAnyKeyword = tsAnyKeyword;
exports.tSBooleanKeyword = exports.TSBooleanKeyword = exports.tsBooleanKeyword = tsBooleanKeyword;
exports.tSBigIntKeyword = exports.TSBigIntKeyword = exports.tsBigIntKeyword = tsBigIntKeyword;
exports.tSIntrinsicKeyword = exports.TSIntrinsicKeyword = exports.tsIntrinsicKeyword = tsIntrinsicKeyword;
exports.tSNeverKeyword = exports.TSNeverKeyword = exports.tsNeverKeyword = tsNeverKeyword;
exports.tSNullKeyword = exports.TSNullKeyword = exports.tsNullKeyword = tsNullKeyword;
exports.tSNumberKeyword = exports.TSNumberKeyword = exports.tsNumberKeyword = tsNumberKeyword;
exports.tSObjectKeyword = exports.TSObjectKeyword = exports.tsObjectKeyword = tsObjectKeyword;
exports.tSStringKeyword = exports.TSStringKeyword = exports.tsStringKeyword = tsStringKeyword;
exports.tSSymbolKeyword = exports.TSSymbolKeyword = exports.tsSymbolKeyword = tsSymbolKeyword;
exports.tSUndefinedKeyword = exports.TSUndefinedKeyword = exports.tsUndefinedKeyword = tsUndefinedKeyword;
exports.tSUnknownKeyword = exports.TSUnknownKeyword = exports.tsUnknownKeyword = tsUnknownKeyword;
exports.tSVoidKeyword = exports.TSVoidKeyword = exports.tsVoidKeyword = tsVoidKeyword;
exports.tSThisType = exports.TSThisType = exports.tsThisType = tsThisType;
exports.tSFunctionType = exports.TSFunctionType = exports.tsFunctionType = tsFunctionType;
exports.tSConstructorType = exports.TSConstructorType = exports.tsConstructorType = tsConstructorType;
exports.tSTypeReference = exports.TSTypeReference = exports.tsTypeReference = tsTypeReference;
exports.tSTypePredicate = exports.TSTypePredicate = exports.tsTypePredicate = tsTypePredicate;
exports.tSTypeQuery = exports.TSTypeQuery = exports.tsTypeQuery = tsTypeQuery;
exports.tSTypeLiteral = exports.TSTypeLiteral = exports.tsTypeLiteral = tsTypeLiteral;
exports.tSArrayType = exports.TSArrayType = exports.tsArrayType = tsArrayType;
exports.tSTupleType = exports.TSTupleType = exports.tsTupleType = tsTupleType;
exports.tSOptionalType = exports.TSOptionalType = exports.tsOptionalType = tsOptionalType;
exports.tSRestType = exports.TSRestType = exports.tsRestType = tsRestType;
exports.tSNamedTupleMember = exports.TSNamedTupleMember = exports.tsNamedTupleMember = tsNamedTupleMember;
exports.tSUnionType = exports.TSUnionType = exports.tsUnionType = tsUnionType;
exports.tSIntersectionType = exports.TSIntersectionType = exports.tsIntersectionType = tsIntersectionType;
exports.tSConditionalType = exports.TSConditionalType = exports.tsConditionalType = tsConditionalType;
exports.tSInferType = exports.TSInferType = exports.tsInferType = tsInferType;
exports.tSParenthesizedType = exports.TSParenthesizedType = exports.tsParenthesizedType = tsParenthesizedType;
exports.tSTypeOperator = exports.TSTypeOperator = exports.tsTypeOperator = tsTypeOperator;
exports.tSIndexedAccessType = exports.TSIndexedAccessType = exports.tsIndexedAccessType = tsIndexedAccessType;
exports.tSMappedType = exports.TSMappedType = exports.tsMappedType = tsMappedType;
exports.tSLiteralType = exports.TSLiteralType = exports.tsLiteralType = tsLiteralType;
exports.tSExpressionWithTypeArguments = exports.TSExpressionWithTypeArguments = exports.tsExpressionWithTypeArguments = tsExpressionWithTypeArguments;
exports.tSInterfaceDeclaration = exports.TSInterfaceDeclaration = exports.tsInterfaceDeclaration = tsInterfaceDeclaration;
exports.tSInterfaceBody = exports.TSInterfaceBody = exports.tsInterfaceBody = tsInterfaceBody;
exports.tSTypeAliasDeclaration = exports.TSTypeAliasDeclaration = exports.tsTypeAliasDeclaration = tsTypeAliasDeclaration;
exports.tSAsExpression = exports.TSAsExpression = exports.tsAsExpression = tsAsExpression;
exports.tSTypeAssertion = exports.TSTypeAssertion = exports.tsTypeAssertion = tsTypeAssertion;
exports.tSEnumDeclaration = exports.TSEnumDeclaration = exports.tsEnumDeclaration = tsEnumDeclaration;
exports.tSEnumMember = exports.TSEnumMember = exports.tsEnumMember = tsEnumMember;
exports.tSModuleDeclaration = exports.TSModuleDeclaration = exports.tsModuleDeclaration = tsModuleDeclaration;
exports.tSModuleBlock = exports.TSModuleBlock = exports.tsModuleBlock = tsModuleBlock;
exports.tSImportType = exports.TSImportType = exports.tsImportType = tsImportType;
exports.tSImportEqualsDeclaration = exports.TSImportEqualsDeclaration = exports.tsImportEqualsDeclaration = tsImportEqualsDeclaration;
exports.tSExternalModuleReference = exports.TSExternalModuleReference = exports.tsExternalModuleReference = tsExternalModuleReference;
exports.tSNonNullExpression = exports.TSNonNullExpression = exports.tsNonNullExpression = tsNonNullExpression;
exports.tSExportAssignment = exports.TSExportAssignment = exports.tsExportAssignment = tsExportAssignment;
exports.tSNamespaceExportDeclaration = exports.TSNamespaceExportDeclaration = exports.tsNamespaceExportDeclaration = tsNamespaceExportDeclaration;
exports.tSTypeAnnotation = exports.TSTypeAnnotation = exports.tsTypeAnnotation = tsTypeAnnotation;
exports.tSTypeParameterInstantiation = exports.TSTypeParameterInstantiation = exports.tsTypeParameterInstantiation = tsTypeParameterInstantiation;
exports.tSTypeParameterDeclaration = exports.TSTypeParameterDeclaration = exports.tsTypeParameterDeclaration = tsTypeParameterDeclaration;
exports.tSTypeParameter = exports.TSTypeParameter = exports.tsTypeParameter = tsTypeParameter;
exports.numberLiteral = exports.NumberLiteral = NumberLiteral;
exports.regexLiteral = exports.RegexLiteral = RegexLiteral;
exports.restProperty = exports.RestProperty = RestProperty;
exports.spreadProperty = exports.SpreadProperty = SpreadProperty;

var _builder = _interopRequireDefault(require("../builder"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function arrayExpression(...args) {
  return (0, _builder.default)("ArrayExpression", ...args);
}

function assignmentExpression(...args) {
  return (0, _builder.default)("AssignmentExpression", ...args);
}

function binaryExpression(...args) {
  return (0, _builder.default)("BinaryExpression", ...args);
}

function interpreterDirective(...args) {
  return (0, _builder.default)("InterpreterDirective", ...args);
}

function directive(...args) {
  return (0, _builder.default)("Directive", ...args);
}

function directiveLiteral(...args) {
  return (0, _builder.default)("DirectiveLiteral", ...args);
}

function blockStatement(...args) {
  return (0, _builder.default)("BlockStatement", ...args);
}

function breakStatement(...args) {
  return (0, _builder.default)("BreakStatement", ...args);
}

function callExpression(...args) {
  return (0, _builder.default)("CallExpression", ...args);
}

function catchClause(...args) {
  return (0, _builder.default)("CatchClause", ...args);
}

function conditionalExpression(...args) {
  return (0, _builder.default)("ConditionalExpression", ...args);
}

function continueStatement(...args) {
  return (0, _builder.default)("ContinueStatement", ...args);
}

function debuggerStatement(...args) {
  return (0, _builder.default)("DebuggerStatement", ...args);
}

function doWhileStatement(...args) {
  return (0, _builder.default)("DoWhileStatement", ...args);
}

function emptyStatement(...args) {
  return (0, _builder.default)("EmptyStatement", ...args);
}

function expressionStatement(...args) {
  return (0, _builder.default)("ExpressionStatement", ...args);
}

function file(...args) {
  return (0, _builder.default)("File", ...args);
}

function forInStatement(...args) {
  return (0, _builder.default)("ForInStatement", ...args);
}

function forStatement(...args) {
  return (0, _builder.default)("ForStatement", ...args);
}

function functionDeclaration(...args) {
  return (0, _builder.default)("FunctionDeclaration", ...args);
}

function functionExpression(...args) {
  return (0, _builder.default)("FunctionExpression", ...args);
}

function identifier(...args) {
  return (0, _builder.default)("Identifier", ...args);
}

function ifStatement(...args) {
  return (0, _builder.default)("IfStatement", ...args);
}

function labeledStatement(...args) {
  return (0, _builder.default)("LabeledStatement", ...args);
}

function stringLiteral(...args) {
  return (0, _builder.default)("StringLiteral", ...args);
}

function numericLiteral(...args) {
  return (0, _builder.default)("NumericLiteral", ...args);
}

function nullLiteral(...args) {
  return (0, _builder.default)("NullLiteral", ...args);
}

function booleanLiteral(...args) {
  return (0, _builder.default)("BooleanLiteral", ...args);
}

function regExpLiteral(...args) {
  return (0, _builder.default)("RegExpLiteral", ...args);
}

function logicalExpression(...args) {
  return (0, _builder.default)("LogicalExpression", ...args);
}

function memberExpression(...args) {
  return (0, _builder.default)("MemberExpression", ...args);
}

function newExpression(...args) {
  return (0, _builder.default)("NewExpression", ...args);
}

function program(...args) {
  return (0, _builder.default)("Program", ...args);
}

function objectExpression(...args) {
  return (0, _builder.default)("ObjectExpression", ...args);
}

function objectMethod(...args) {
  return (0, _builder.default)("ObjectMethod", ...args);
}

function objectProperty(...args) {
  return (0, _builder.default)("ObjectProperty", ...args);
}

function restElement(...args) {
  return (0, _builder.default)("RestElement", ...args);
}

function returnStatement(...args) {
  return (0, _builder.default)("ReturnStatement", ...args);
}

function sequenceExpression(...args) {
  return (0, _builder.default)("SequenceExpression", ...args);
}

function parenthesizedExpression(...args) {
  return (0, _builder.default)("ParenthesizedExpression", ...args);
}

function switchCase(...args) {
  return (0, _builder.default)("SwitchCase", ...args);
}

function switchStatement(...args) {
  return (0, _builder.default)("SwitchStatement", ...args);
}

function thisExpression(...args) {
  return (0, _builder.default)("ThisExpression", ...args);
}

function throwStatement(...args) {
  return (0, _builder.default)("ThrowStatement", ...args);
}

function tryStatement(...args) {
  return (0, _builder.default)("TryStatement", ...args);
}

function unaryExpression(...args) {
  return (0, _builder.default)("UnaryExpression", ...args);
}

function updateExpression(...args) {
  return (0, _builder.default)("UpdateExpression", ...args);
}

function variableDeclaration(...args) {
  return (0, _builder.default)("VariableDeclaration", ...args);
}

function variableDeclarator(...args) {
  return (0, _builder.default)("VariableDeclarator", ...args);
}

function whileStatement(...args) {
  return (0, _builder.default)("WhileStatement", ...args);
}

function withStatement(...args) {
  return (0, _builder.default)("WithStatement", ...args);
}

function assignmentPattern(...args) {
  return (0, _builder.default)("AssignmentPattern", ...args);
}

function arrayPattern(...args) {
  return (0, _builder.default)("ArrayPattern", ...args);
}

function arrowFunctionExpression(...args) {
  return (0, _builder.default)("ArrowFunctionExpression", ...args);
}

function classBody(...args) {
  return (0, _builder.default)("ClassBody", ...args);
}

function classExpression(...args) {
  return (0, _builder.default)("ClassExpression", ...args);
}

function classDeclaration(...args) {
  return (0, _builder.default)("ClassDeclaration", ...args);
}

function exportAllDeclaration(...args) {
  return (0, _builder.default)("ExportAllDeclaration", ...args);
}

function exportDefaultDeclaration(...args) {
  return (0, _builder.default)("ExportDefaultDeclaration", ...args);
}

function exportNamedDeclaration(...args) {
  return (0, _builder.default)("ExportNamedDeclaration", ...args);
}

function exportSpecifier(...args) {
  return (0, _builder.default)("ExportSpecifier", ...args);
}

function forOfStatement(...args) {
  return (0, _builder.default)("ForOfStatement", ...args);
}

function importDeclaration(...args) {
  return (0, _builder.default)("ImportDeclaration", ...args);
}

function importDefaultSpecifier(...args) {
  return (0, _builder.default)("ImportDefaultSpecifier", ...args);
}

function importNamespaceSpecifier(...args) {
  return (0, _builder.default)("ImportNamespaceSpecifier", ...args);
}

function importSpecifier(...args) {
  return (0, _builder.default)("ImportSpecifier", ...args);
}

function metaProperty(...args) {
  return (0, _builder.default)("MetaProperty", ...args);
}

function classMethod(...args) {
  return (0, _builder.default)("ClassMethod", ...args);
}

function objectPattern(...args) {
  return (0, _builder.default)("ObjectPattern", ...args);
}

function spreadElement(...args) {
  return (0, _builder.default)("SpreadElement", ...args);
}

function _super(...args) {
  return (0, _builder.default)("Super", ...args);
}

function taggedTemplateExpression(...args) {
  return (0, _builder.default)("TaggedTemplateExpression", ...args);
}

function templateElement(...args) {
  return (0, _builder.default)("TemplateElement", ...args);
}

function templateLiteral(...args) {
  return (0, _builder.default)("TemplateLiteral", ...args);
}

function yieldExpression(...args) {
  return (0, _builder.default)("YieldExpression", ...args);
}

function awaitExpression(...args) {
  return (0, _builder.default)("AwaitExpression", ...args);
}

function _import(...args) {
  return (0, _builder.default)("Import", ...args);
}

function bigIntLiteral(...args) {
  return (0, _builder.default)("BigIntLiteral", ...args);
}

function exportNamespaceSpecifier(...args) {
  return (0, _builder.default)("ExportNamespaceSpecifier", ...args);
}

function optionalMemberExpression(...args) {
  return (0, _builder.default)("OptionalMemberExpression", ...args);
}

function optionalCallExpression(...args) {
  return (0, _builder.default)("OptionalCallExpression", ...args);
}

function anyTypeAnnotation(...args) {
  return (0, _builder.default)("AnyTypeAnnotation", ...args);
}

function arrayTypeAnnotation(...args) {
  return (0, _builder.default)("ArrayTypeAnnotation", ...args);
}

function booleanTypeAnnotation(...args) {
  return (0, _builder.default)("BooleanTypeAnnotation", ...args);
}

function booleanLiteralTypeAnnotation(...args) {
  return (0, _builder.default)("BooleanLiteralTypeAnnotation", ...args);
}

function nullLiteralTypeAnnotation(...args) {
  return (0, _builder.default)("NullLiteralTypeAnnotation", ...args);
}

function classImplements(...args) {
  return (0, _builder.default)("ClassImplements", ...args);
}

function declareClass(...args) {
  return (0, _builder.default)("DeclareClass", ...args);
}

function declareFunction(...args) {
  return (0, _builder.default)("DeclareFunction", ...args);
}

function declareInterface(...args) {
  return (0, _builder.default)("DeclareInterface", ...args);
}

function declareModule(...args) {
  return (0, _builder.default)("DeclareModule", ...args);
}

function declareModuleExports(...args) {
  return (0, _builder.default)("DeclareModuleExports", ...args);
}

function declareTypeAlias(...args) {
  return (0, _builder.default)("DeclareTypeAlias", ...args);
}

function declareOpaqueType(...args) {
  return (0, _builder.default)("DeclareOpaqueType", ...args);
}

function declareVariable(...args) {
  return (0, _builder.default)("DeclareVariable", ...args);
}

function declareExportDeclaration(...args) {
  return (0, _builder.default)("DeclareExportDeclaration", ...args);
}

function declareExportAllDeclaration(...args) {
  return (0, _builder.default)("DeclareExportAllDeclaration", ...args);
}

function declaredPredicate(...args) {
  return (0, _builder.default)("DeclaredPredicate", ...args);
}

function existsTypeAnnotation(...args) {
  return (0, _builder.default)("ExistsTypeAnnotation", ...args);
}

function functionTypeAnnotation(...args) {
  return (0, _builder.default)("FunctionTypeAnnotation", ...args);
}

function functionTypeParam(...args) {
  return (0, _builder.default)("FunctionTypeParam", ...args);
}

function genericTypeAnnotation(...args) {
  return (0, _builder.default)("GenericTypeAnnotation", ...args);
}

function inferredPredicate(...args) {
  return (0, _builder.default)("InferredPredicate", ...args);
}

function interfaceExtends(...args) {
  return (0, _builder.default)("InterfaceExtends", ...args);
}

function interfaceDeclaration(...args) {
  return (0, _builder.default)("InterfaceDeclaration", ...args);
}

function interfaceTypeAnnotation(...args) {
  return (0, _builder.default)("InterfaceTypeAnnotation", ...args);
}

function intersectionTypeAnnotation(...args) {
  return (0, _builder.default)("IntersectionTypeAnnotation", ...args);
}

function mixedTypeAnnotation(...args) {
  return (0, _builder.default)("MixedTypeAnnotation", ...args);
}

function emptyTypeAnnotation(...args) {
  return (0, _builder.default)("EmptyTypeAnnotation", ...args);
}

function nullableTypeAnnotation(...args) {
  return (0, _builder.default)("NullableTypeAnnotation", ...args);
}

function numberLiteralTypeAnnotation(...args) {
  return (0, _builder.default)("NumberLiteralTypeAnnotation", ...args);
}

function numberTypeAnnotation(...args) {
  return (0, _builder.default)("NumberTypeAnnotation", ...args);
}

function objectTypeAnnotation(...args) {
  return (0, _builder.default)("ObjectTypeAnnotation", ...args);
}

function objectTypeInternalSlot(...args) {
  return (0, _builder.default)("ObjectTypeInternalSlot", ...args);
}

function objectTypeCallProperty(...args) {
  return (0, _builder.default)("ObjectTypeCallProperty", ...args);
}

function objectTypeIndexer(...args) {
  return (0, _builder.default)("ObjectTypeIndexer", ...args);
}

function objectTypeProperty(...args) {
  return (0, _builder.default)("ObjectTypeProperty", ...args);
}

function objectTypeSpreadProperty(...args) {
  return (0, _builder.default)("ObjectTypeSpreadProperty", ...args);
}

function opaqueType(...args) {
  return (0, _builder.default)("OpaqueType", ...args);
}

function qualifiedTypeIdentifier(...args) {
  return (0, _builder.default)("QualifiedTypeIdentifier", ...args);
}

function stringLiteralTypeAnnotation(...args) {
  return (0, _builder.default)("StringLiteralTypeAnnotation", ...args);
}

function stringTypeAnnotation(...args) {
  return (0, _builder.default)("StringTypeAnnotation", ...args);
}

function symbolTypeAnnotation(...args) {
  return (0, _builder.default)("SymbolTypeAnnotation", ...args);
}

function thisTypeAnnotation(...args) {
  return (0, _builder.default)("ThisTypeAnnotation", ...args);
}

function tupleTypeAnnotation(...args) {
  return (0, _builder.default)("TupleTypeAnnotation", ...args);
}

function typeofTypeAnnotation(...args) {
  return (0, _builder.default)("TypeofTypeAnnotation", ...args);
}

function typeAlias(...args) {
  return (0, _builder.default)("TypeAlias", ...args);
}

function typeAnnotation(...args) {
  return (0, _builder.default)("TypeAnnotation", ...args);
}

function typeCastExpression(...args) {
  return (0, _builder.default)("TypeCastExpression", ...args);
}

function typeParameter(...args) {
  return (0, _builder.default)("TypeParameter", ...args);
}

function typeParameterDeclaration(...args) {
  return (0, _builder.default)("TypeParameterDeclaration", ...args);
}

function typeParameterInstantiation(...args) {
  return (0, _builder.default)("TypeParameterInstantiation", ...args);
}

function unionTypeAnnotation(...args) {
  return (0, _builder.default)("UnionTypeAnnotation", ...args);
}

function variance(...args) {
  return (0, _builder.default)("Variance", ...args);
}

function voidTypeAnnotation(...args) {
  return (0, _builder.default)("VoidTypeAnnotation", ...args);
}

function enumDeclaration(...args) {
  return (0, _builder.default)("EnumDeclaration", ...args);
}

function enumBooleanBody(...args) {
  return (0, _builder.default)("EnumBooleanBody", ...args);
}

function enumNumberBody(...args) {
  return (0, _builder.default)("EnumNumberBody", ...args);
}

function enumStringBody(...args) {
  return (0, _builder.default)("EnumStringBody", ...args);
}

function enumSymbolBody(...args) {
  return (0, _builder.default)("EnumSymbolBody", ...args);
}

function enumBooleanMember(...args) {
  return (0, _builder.default)("EnumBooleanMember", ...args);
}

function enumNumberMember(...args) {
  return (0, _builder.default)("EnumNumberMember", ...args);
}

function enumStringMember(...args) {
  return (0, _builder.default)("EnumStringMember", ...args);
}

function enumDefaultedMember(...args) {
  return (0, _builder.default)("EnumDefaultedMember", ...args);
}

function jsxAttribute(...args) {
  return (0, _builder.default)("JSXAttribute", ...args);
}

function jsxClosingElement(...args) {
  return (0, _builder.default)("JSXClosingElement", ...args);
}

function jsxElement(...args) {
  return (0, _builder.default)("JSXElement", ...args);
}

function jsxEmptyExpression(...args) {
  return (0, _builder.default)("JSXEmptyExpression", ...args);
}

function jsxExpressionContainer(...args) {
  return (0, _builder.default)("JSXExpressionContainer", ...args);
}

function jsxSpreadChild(...args) {
  return (0, _builder.default)("JSXSpreadChild", ...args);
}

function jsxIdentifier(...args) {
  return (0, _builder.default)("JSXIdentifier", ...args);
}

function jsxMemberExpression(...args) {
  return (0, _builder.default)("JSXMemberExpression", ...args);
}

function jsxNamespacedName(...args) {
  return (0, _builder.default)("JSXNamespacedName", ...args);
}

function jsxOpeningElement(...args) {
  return (0, _builder.default)("JSXOpeningElement", ...args);
}

function jsxSpreadAttribute(...args) {
  return (0, _builder.default)("JSXSpreadAttribute", ...args);
}

function jsxText(...args) {
  return (0, _builder.default)("JSXText", ...args);
}

function jsxFragment(...args) {
  return (0, _builder.default)("JSXFragment", ...args);
}

function jsxOpeningFragment(...args) {
  return (0, _builder.default)("JSXOpeningFragment", ...args);
}

function jsxClosingFragment(...args) {
  return (0, _builder.default)("JSXClosingFragment", ...args);
}

function noop(...args) {
  return (0, _builder.default)("Noop", ...args);
}

function placeholder(...args) {
  return (0, _builder.default)("Placeholder", ...args);
}

function v8IntrinsicIdentifier(...args) {
  return (0, _builder.default)("V8IntrinsicIdentifier", ...args);
}

function argumentPlaceholder(...args) {
  return (0, _builder.default)("ArgumentPlaceholder", ...args);
}

function bindExpression(...args) {
  return (0, _builder.default)("BindExpression", ...args);
}

function classProperty(...args) {
  return (0, _builder.default)("ClassProperty", ...args);
}

function pipelineTopicExpression(...args) {
  return (0, _builder.default)("PipelineTopicExpression", ...args);
}

function pipelineBareFunction(...args) {
  return (0, _builder.default)("PipelineBareFunction", ...args);
}

function pipelinePrimaryTopicReference(...args) {
  return (0, _builder.default)("PipelinePrimaryTopicReference", ...args);
}

function classPrivateProperty(...args) {
  return (0, _builder.default)("ClassPrivateProperty", ...args);
}

function classPrivateMethod(...args) {
  return (0, _builder.default)("ClassPrivateMethod", ...args);
}

function importAttribute(...args) {
  return (0, _builder.default)("ImportAttribute", ...args);
}

function decorator(...args) {
  return (0, _builder.default)("Decorator", ...args);
}

function doExpression(...args) {
  return (0, _builder.default)("DoExpression", ...args);
}

function exportDefaultSpecifier(...args) {
  return (0, _builder.default)("ExportDefaultSpecifier", ...args);
}

function privateName(...args) {
  return (0, _builder.default)("PrivateName", ...args);
}

function recordExpression(...args) {
  return (0, _builder.default)("RecordExpression", ...args);
}

function tupleExpression(...args) {
  return (0, _builder.default)("TupleExpression", ...args);
}

function decimalLiteral(...args) {
  return (0, _builder.default)("DecimalLiteral", ...args);
}

function staticBlock(...args) {
  return (0, _builder.default)("StaticBlock", ...args);
}

function tsParameterProperty(...args) {
  return (0, _builder.default)("TSParameterProperty", ...args);
}

function tsDeclareFunction(...args) {
  return (0, _builder.default)("TSDeclareFunction", ...args);
}

function tsDeclareMethod(...args) {
  return (0, _builder.default)("TSDeclareMethod", ...args);
}

function tsQualifiedName(...args) {
  return (0, _builder.default)("TSQualifiedName", ...args);
}

function tsCallSignatureDeclaration(...args) {
  return (0, _builder.default)("TSCallSignatureDeclaration", ...args);
}

function tsConstructSignatureDeclaration(...args) {
  return (0, _builder.default)("TSConstructSignatureDeclaration", ...args);
}

function tsPropertySignature(...args) {
  return (0, _builder.default)("TSPropertySignature", ...args);
}

function tsMethodSignature(...args) {
  return (0, _builder.default)("TSMethodSignature", ...args);
}

function tsIndexSignature(...args) {
  return (0, _builder.default)("TSIndexSignature", ...args);
}

function tsAnyKeyword(...args) {
  return (0, _builder.default)("TSAnyKeyword", ...args);
}

function tsBooleanKeyword(...args) {
  return (0, _builder.default)("TSBooleanKeyword", ...args);
}

function tsBigIntKeyword(...args) {
  return (0, _builder.default)("TSBigIntKeyword", ...args);
}

function tsIntrinsicKeyword(...args) {
  return (0, _builder.default)("TSIntrinsicKeyword", ...args);
}

function tsNeverKeyword(...args) {
  return (0, _builder.default)("TSNeverKeyword", ...args);
}

function tsNullKeyword(...args) {
  return (0, _builder.default)("TSNullKeyword", ...args);
}

function tsNumberKeyword(...args) {
  return (0, _builder.default)("TSNumberKeyword", ...args);
}

function tsObjectKeyword(...args) {
  return (0, _builder.default)("TSObjectKeyword", ...args);
}

function tsStringKeyword(...args) {
  return (0, _builder.default)("TSStringKeyword", ...args);
}

function tsSymbolKeyword(...args) {
  return (0, _builder.default)("TSSymbolKeyword", ...args);
}

function tsUndefinedKeyword(...args) {
  return (0, _builder.default)("TSUndefinedKeyword", ...args);
}

function tsUnknownKeyword(...args) {
  return (0, _builder.default)("TSUnknownKeyword", ...args);
}

function tsVoidKeyword(...args) {
  return (0, _builder.default)("TSVoidKeyword", ...args);
}

function tsThisType(...args) {
  return (0, _builder.default)("TSThisType", ...args);
}

function tsFunctionType(...args) {
  return (0, _builder.default)("TSFunctionType", ...args);
}

function tsConstructorType(...args) {
  return (0, _builder.default)("TSConstructorType", ...args);
}

function tsTypeReference(...args) {
  return (0, _builder.default)("TSTypeReference", ...args);
}

function tsTypePredicate(...args) {
  return (0, _builder.default)("TSTypePredicate", ...args);
}

function tsTypeQuery(...args) {
  return (0, _builder.default)("TSTypeQuery", ...args);
}

function tsTypeLiteral(...args) {
  return (0, _builder.default)("TSTypeLiteral", ...args);
}

function tsArrayType(...args) {
  return (0, _builder.default)("TSArrayType", ...args);
}

function tsTupleType(...args) {
  return (0, _builder.default)("TSTupleType", ...args);
}

function tsOptionalType(...args) {
  return (0, _builder.default)("TSOptionalType", ...args);
}

function tsRestType(...args) {
  return (0, _builder.default)("TSRestType", ...args);
}

function tsNamedTupleMember(...args) {
  return (0, _builder.default)("TSNamedTupleMember", ...args);
}

function tsUnionType(...args) {
  return (0, _builder.default)("TSUnionType", ...args);
}

function tsIntersectionType(...args) {
  return (0, _builder.default)("TSIntersectionType", ...args);
}

function tsConditionalType(...args) {
  return (0, _builder.default)("TSConditionalType", ...args);
}

function tsInferType(...args) {
  return (0, _builder.default)("TSInferType", ...args);
}

function tsParenthesizedType(...args) {
  return (0, _builder.default)("TSParenthesizedType", ...args);
}

function tsTypeOperator(...args) {
  return (0, _builder.default)("TSTypeOperator", ...args);
}

function tsIndexedAccessType(...args) {
  return (0, _builder.default)("TSIndexedAccessType", ...args);
}

function tsMappedType(...args) {
  return (0, _builder.default)("TSMappedType", ...args);
}

function tsLiteralType(...args) {
  return (0, _builder.default)("TSLiteralType", ...args);
}

function tsExpressionWithTypeArguments(...args) {
  return (0, _builder.default)("TSExpressionWithTypeArguments", ...args);
}

function tsInterfaceDeclaration(...args) {
  return (0, _builder.default)("TSInterfaceDeclaration", ...args);
}

function tsInterfaceBody(...args) {
  return (0, _builder.default)("TSInterfaceBody", ...args);
}

function tsTypeAliasDeclaration(...args) {
  return (0, _builder.default)("TSTypeAliasDeclaration", ...args);
}

function tsAsExpression(...args) {
  return (0, _builder.default)("TSAsExpression", ...args);
}

function tsTypeAssertion(...args) {
  return (0, _builder.default)("TSTypeAssertion", ...args);
}

function tsEnumDeclaration(...args) {
  return (0, _builder.default)("TSEnumDeclaration", ...args);
}

function tsEnumMember(...args) {
  return (0, _builder.default)("TSEnumMember", ...args);
}

function tsModuleDeclaration(...args) {
  return (0, _builder.default)("TSModuleDeclaration", ...args);
}

function tsModuleBlock(...args) {
  return (0, _builder.default)("TSModuleBlock", ...args);
}

function tsImportType(...args) {
  return (0, _builder.default)("TSImportType", ...args);
}

function tsImportEqualsDeclaration(...args) {
  return (0, _builder.default)("TSImportEqualsDeclaration", ...args);
}

function tsExternalModuleReference(...args) {
  return (0, _builder.default)("TSExternalModuleReference", ...args);
}

function tsNonNullExpression(...args) {
  return (0, _builder.default)("TSNonNullExpression", ...args);
}

function tsExportAssignment(...args) {
  return (0, _builder.default)("TSExportAssignment", ...args);
}

function tsNamespaceExportDeclaration(...args) {
  return (0, _builder.default)("TSNamespaceExportDeclaration", ...args);
}

function tsTypeAnnotation(...args) {
  return (0, _builder.default)("TSTypeAnnotation", ...args);
}

function tsTypeParameterInstantiation(...args) {
  return (0, _builder.default)("TSTypeParameterInstantiation", ...args);
}

function tsTypeParameterDeclaration(...args) {
  return (0, _builder.default)("TSTypeParameterDeclaration", ...args);
}

function tsTypeParameter(...args) {
  return (0, _builder.default)("TSTypeParameter", ...args);
}

function NumberLiteral(...args) {
  console.trace("The node type NumberLiteral has been renamed to NumericLiteral");
  return (0, _builder.default)("NumberLiteral", ...args);
}

function RegexLiteral(...args) {
  console.trace("The node type RegexLiteral has been renamed to RegExpLiteral");
  return (0, _builder.default)("RegexLiteral", ...args);
}

function RestProperty(...args) {
  console.trace("The node type RestProperty has been renamed to RestElement");
  return (0, _builder.default)("RestProperty", ...args);
}

function SpreadProperty(...args) {
  console.trace("The node type SpreadProperty has been renamed to SpreadElement");
  return (0, _builder.default)("SpreadProperty", ...args);
}