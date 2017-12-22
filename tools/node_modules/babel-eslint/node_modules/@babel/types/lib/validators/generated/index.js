"use strict";

exports.__esModule = true;
exports.isArrayExpression = isArrayExpression;
exports.isAssignmentExpression = isAssignmentExpression;
exports.isBinaryExpression = isBinaryExpression;
exports.isDirective = isDirective;
exports.isDirectiveLiteral = isDirectiveLiteral;
exports.isBlockStatement = isBlockStatement;
exports.isBreakStatement = isBreakStatement;
exports.isCallExpression = isCallExpression;
exports.isCatchClause = isCatchClause;
exports.isConditionalExpression = isConditionalExpression;
exports.isContinueStatement = isContinueStatement;
exports.isDebuggerStatement = isDebuggerStatement;
exports.isDoWhileStatement = isDoWhileStatement;
exports.isEmptyStatement = isEmptyStatement;
exports.isExpressionStatement = isExpressionStatement;
exports.isFile = isFile;
exports.isForInStatement = isForInStatement;
exports.isForStatement = isForStatement;
exports.isFunctionDeclaration = isFunctionDeclaration;
exports.isFunctionExpression = isFunctionExpression;
exports.isIdentifier = isIdentifier;
exports.isIfStatement = isIfStatement;
exports.isLabeledStatement = isLabeledStatement;
exports.isStringLiteral = isStringLiteral;
exports.isNumericLiteral = isNumericLiteral;
exports.isNullLiteral = isNullLiteral;
exports.isBooleanLiteral = isBooleanLiteral;
exports.isRegExpLiteral = isRegExpLiteral;
exports.isLogicalExpression = isLogicalExpression;
exports.isMemberExpression = isMemberExpression;
exports.isNewExpression = isNewExpression;
exports.isProgram = isProgram;
exports.isObjectExpression = isObjectExpression;
exports.isObjectMethod = isObjectMethod;
exports.isObjectProperty = isObjectProperty;
exports.isRestElement = isRestElement;
exports.isReturnStatement = isReturnStatement;
exports.isSequenceExpression = isSequenceExpression;
exports.isSwitchCase = isSwitchCase;
exports.isSwitchStatement = isSwitchStatement;
exports.isThisExpression = isThisExpression;
exports.isThrowStatement = isThrowStatement;
exports.isTryStatement = isTryStatement;
exports.isUnaryExpression = isUnaryExpression;
exports.isUpdateExpression = isUpdateExpression;
exports.isVariableDeclaration = isVariableDeclaration;
exports.isVariableDeclarator = isVariableDeclarator;
exports.isWhileStatement = isWhileStatement;
exports.isWithStatement = isWithStatement;
exports.isAssignmentPattern = isAssignmentPattern;
exports.isArrayPattern = isArrayPattern;
exports.isArrowFunctionExpression = isArrowFunctionExpression;
exports.isClassBody = isClassBody;
exports.isClassDeclaration = isClassDeclaration;
exports.isClassExpression = isClassExpression;
exports.isExportAllDeclaration = isExportAllDeclaration;
exports.isExportDefaultDeclaration = isExportDefaultDeclaration;
exports.isExportNamedDeclaration = isExportNamedDeclaration;
exports.isExportSpecifier = isExportSpecifier;
exports.isForOfStatement = isForOfStatement;
exports.isImportDeclaration = isImportDeclaration;
exports.isImportDefaultSpecifier = isImportDefaultSpecifier;
exports.isImportNamespaceSpecifier = isImportNamespaceSpecifier;
exports.isImportSpecifier = isImportSpecifier;
exports.isMetaProperty = isMetaProperty;
exports.isClassMethod = isClassMethod;
exports.isObjectPattern = isObjectPattern;
exports.isSpreadElement = isSpreadElement;
exports.isSuper = isSuper;
exports.isTaggedTemplateExpression = isTaggedTemplateExpression;
exports.isTemplateElement = isTemplateElement;
exports.isTemplateLiteral = isTemplateLiteral;
exports.isYieldExpression = isYieldExpression;
exports.isAnyTypeAnnotation = isAnyTypeAnnotation;
exports.isArrayTypeAnnotation = isArrayTypeAnnotation;
exports.isBooleanTypeAnnotation = isBooleanTypeAnnotation;
exports.isBooleanLiteralTypeAnnotation = isBooleanLiteralTypeAnnotation;
exports.isNullLiteralTypeAnnotation = isNullLiteralTypeAnnotation;
exports.isClassImplements = isClassImplements;
exports.isDeclareClass = isDeclareClass;
exports.isDeclareFunction = isDeclareFunction;
exports.isDeclareInterface = isDeclareInterface;
exports.isDeclareModule = isDeclareModule;
exports.isDeclareModuleExports = isDeclareModuleExports;
exports.isDeclareTypeAlias = isDeclareTypeAlias;
exports.isDeclareOpaqueType = isDeclareOpaqueType;
exports.isDeclareVariable = isDeclareVariable;
exports.isDeclareExportDeclaration = isDeclareExportDeclaration;
exports.isDeclareExportAllDeclaration = isDeclareExportAllDeclaration;
exports.isDeclaredPredicate = isDeclaredPredicate;
exports.isExistsTypeAnnotation = isExistsTypeAnnotation;
exports.isFunctionTypeAnnotation = isFunctionTypeAnnotation;
exports.isFunctionTypeParam = isFunctionTypeParam;
exports.isGenericTypeAnnotation = isGenericTypeAnnotation;
exports.isInferredPredicate = isInferredPredicate;
exports.isInterfaceExtends = isInterfaceExtends;
exports.isInterfaceDeclaration = isInterfaceDeclaration;
exports.isIntersectionTypeAnnotation = isIntersectionTypeAnnotation;
exports.isMixedTypeAnnotation = isMixedTypeAnnotation;
exports.isEmptyTypeAnnotation = isEmptyTypeAnnotation;
exports.isNullableTypeAnnotation = isNullableTypeAnnotation;
exports.isNumberLiteralTypeAnnotation = isNumberLiteralTypeAnnotation;
exports.isNumberTypeAnnotation = isNumberTypeAnnotation;
exports.isObjectTypeAnnotation = isObjectTypeAnnotation;
exports.isObjectTypeCallProperty = isObjectTypeCallProperty;
exports.isObjectTypeIndexer = isObjectTypeIndexer;
exports.isObjectTypeProperty = isObjectTypeProperty;
exports.isObjectTypeSpreadProperty = isObjectTypeSpreadProperty;
exports.isOpaqueType = isOpaqueType;
exports.isQualifiedTypeIdentifier = isQualifiedTypeIdentifier;
exports.isStringLiteralTypeAnnotation = isStringLiteralTypeAnnotation;
exports.isStringTypeAnnotation = isStringTypeAnnotation;
exports.isThisTypeAnnotation = isThisTypeAnnotation;
exports.isTupleTypeAnnotation = isTupleTypeAnnotation;
exports.isTypeofTypeAnnotation = isTypeofTypeAnnotation;
exports.isTypeAlias = isTypeAlias;
exports.isTypeAnnotation = isTypeAnnotation;
exports.isTypeCastExpression = isTypeCastExpression;
exports.isTypeParameter = isTypeParameter;
exports.isTypeParameterDeclaration = isTypeParameterDeclaration;
exports.isTypeParameterInstantiation = isTypeParameterInstantiation;
exports.isUnionTypeAnnotation = isUnionTypeAnnotation;
exports.isVoidTypeAnnotation = isVoidTypeAnnotation;
exports.isJSXAttribute = isJSXAttribute;
exports.isJSXClosingElement = isJSXClosingElement;
exports.isJSXElement = isJSXElement;
exports.isJSXEmptyExpression = isJSXEmptyExpression;
exports.isJSXExpressionContainer = isJSXExpressionContainer;
exports.isJSXSpreadChild = isJSXSpreadChild;
exports.isJSXIdentifier = isJSXIdentifier;
exports.isJSXMemberExpression = isJSXMemberExpression;
exports.isJSXNamespacedName = isJSXNamespacedName;
exports.isJSXOpeningElement = isJSXOpeningElement;
exports.isJSXSpreadAttribute = isJSXSpreadAttribute;
exports.isJSXText = isJSXText;
exports.isJSXFragment = isJSXFragment;
exports.isJSXOpeningFragment = isJSXOpeningFragment;
exports.isJSXClosingFragment = isJSXClosingFragment;
exports.isNoop = isNoop;
exports.isParenthesizedExpression = isParenthesizedExpression;
exports.isAwaitExpression = isAwaitExpression;
exports.isBindExpression = isBindExpression;
exports.isClassProperty = isClassProperty;
exports.isImport = isImport;
exports.isDecorator = isDecorator;
exports.isDoExpression = isDoExpression;
exports.isExportDefaultSpecifier = isExportDefaultSpecifier;
exports.isExportNamespaceSpecifier = isExportNamespaceSpecifier;
exports.isTSParameterProperty = isTSParameterProperty;
exports.isTSDeclareFunction = isTSDeclareFunction;
exports.isTSDeclareMethod = isTSDeclareMethod;
exports.isTSQualifiedName = isTSQualifiedName;
exports.isTSCallSignatureDeclaration = isTSCallSignatureDeclaration;
exports.isTSConstructSignatureDeclaration = isTSConstructSignatureDeclaration;
exports.isTSPropertySignature = isTSPropertySignature;
exports.isTSMethodSignature = isTSMethodSignature;
exports.isTSIndexSignature = isTSIndexSignature;
exports.isTSAnyKeyword = isTSAnyKeyword;
exports.isTSNumberKeyword = isTSNumberKeyword;
exports.isTSObjectKeyword = isTSObjectKeyword;
exports.isTSBooleanKeyword = isTSBooleanKeyword;
exports.isTSStringKeyword = isTSStringKeyword;
exports.isTSSymbolKeyword = isTSSymbolKeyword;
exports.isTSVoidKeyword = isTSVoidKeyword;
exports.isTSUndefinedKeyword = isTSUndefinedKeyword;
exports.isTSNullKeyword = isTSNullKeyword;
exports.isTSNeverKeyword = isTSNeverKeyword;
exports.isTSThisType = isTSThisType;
exports.isTSFunctionType = isTSFunctionType;
exports.isTSConstructorType = isTSConstructorType;
exports.isTSTypeReference = isTSTypeReference;
exports.isTSTypePredicate = isTSTypePredicate;
exports.isTSTypeQuery = isTSTypeQuery;
exports.isTSTypeLiteral = isTSTypeLiteral;
exports.isTSArrayType = isTSArrayType;
exports.isTSTupleType = isTSTupleType;
exports.isTSUnionType = isTSUnionType;
exports.isTSIntersectionType = isTSIntersectionType;
exports.isTSParenthesizedType = isTSParenthesizedType;
exports.isTSTypeOperator = isTSTypeOperator;
exports.isTSIndexedAccessType = isTSIndexedAccessType;
exports.isTSMappedType = isTSMappedType;
exports.isTSLiteralType = isTSLiteralType;
exports.isTSExpressionWithTypeArguments = isTSExpressionWithTypeArguments;
exports.isTSInterfaceDeclaration = isTSInterfaceDeclaration;
exports.isTSInterfaceBody = isTSInterfaceBody;
exports.isTSTypeAliasDeclaration = isTSTypeAliasDeclaration;
exports.isTSAsExpression = isTSAsExpression;
exports.isTSTypeAssertion = isTSTypeAssertion;
exports.isTSEnumDeclaration = isTSEnumDeclaration;
exports.isTSEnumMember = isTSEnumMember;
exports.isTSModuleDeclaration = isTSModuleDeclaration;
exports.isTSModuleBlock = isTSModuleBlock;
exports.isTSImportEqualsDeclaration = isTSImportEqualsDeclaration;
exports.isTSExternalModuleReference = isTSExternalModuleReference;
exports.isTSNonNullExpression = isTSNonNullExpression;
exports.isTSExportAssignment = isTSExportAssignment;
exports.isTSNamespaceExportDeclaration = isTSNamespaceExportDeclaration;
exports.isTSTypeAnnotation = isTSTypeAnnotation;
exports.isTSTypeParameterInstantiation = isTSTypeParameterInstantiation;
exports.isTSTypeParameterDeclaration = isTSTypeParameterDeclaration;
exports.isTSTypeParameter = isTSTypeParameter;
exports.isExpression = isExpression;
exports.isBinary = isBinary;
exports.isScopable = isScopable;
exports.isBlockParent = isBlockParent;
exports.isBlock = isBlock;
exports.isStatement = isStatement;
exports.isTerminatorless = isTerminatorless;
exports.isCompletionStatement = isCompletionStatement;
exports.isConditional = isConditional;
exports.isLoop = isLoop;
exports.isWhile = isWhile;
exports.isExpressionWrapper = isExpressionWrapper;
exports.isFor = isFor;
exports.isForXStatement = isForXStatement;
exports.isFunction = isFunction;
exports.isFunctionParent = isFunctionParent;
exports.isPureish = isPureish;
exports.isDeclaration = isDeclaration;
exports.isPatternLike = isPatternLike;
exports.isLVal = isLVal;
exports.isTSEntityName = isTSEntityName;
exports.isLiteral = isLiteral;
exports.isImmutable = isImmutable;
exports.isUserWhitespacable = isUserWhitespacable;
exports.isMethod = isMethod;
exports.isObjectMember = isObjectMember;
exports.isProperty = isProperty;
exports.isUnaryLike = isUnaryLike;
exports.isPattern = isPattern;
exports.isClass = isClass;
exports.isModuleDeclaration = isModuleDeclaration;
exports.isExportDeclaration = isExportDeclaration;
exports.isModuleSpecifier = isModuleSpecifier;
exports.isFlow = isFlow;
exports.isFlowBaseAnnotation = isFlowBaseAnnotation;
exports.isFlowDeclaration = isFlowDeclaration;
exports.isFlowPredicate = isFlowPredicate;
exports.isJSX = isJSX;
exports.isTSTypeElement = isTSTypeElement;
exports.isTSType = isTSType;
exports.isNumberLiteral = isNumberLiteral;
exports.isRegexLiteral = isRegexLiteral;
exports.isRestProperty = isRestProperty;
exports.isSpreadProperty = isSpreadProperty;

var _is = _interopRequireDefault(require("../is"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function isArrayExpression(node, opts) {
  return (0, _is.default)("ArrayExpression", node, opts);
}

function isAssignmentExpression(node, opts) {
  return (0, _is.default)("AssignmentExpression", node, opts);
}

function isBinaryExpression(node, opts) {
  return (0, _is.default)("BinaryExpression", node, opts);
}

function isDirective(node, opts) {
  return (0, _is.default)("Directive", node, opts);
}

function isDirectiveLiteral(node, opts) {
  return (0, _is.default)("DirectiveLiteral", node, opts);
}

function isBlockStatement(node, opts) {
  return (0, _is.default)("BlockStatement", node, opts);
}

function isBreakStatement(node, opts) {
  return (0, _is.default)("BreakStatement", node, opts);
}

function isCallExpression(node, opts) {
  return (0, _is.default)("CallExpression", node, opts);
}

function isCatchClause(node, opts) {
  return (0, _is.default)("CatchClause", node, opts);
}

function isConditionalExpression(node, opts) {
  return (0, _is.default)("ConditionalExpression", node, opts);
}

function isContinueStatement(node, opts) {
  return (0, _is.default)("ContinueStatement", node, opts);
}

function isDebuggerStatement(node, opts) {
  return (0, _is.default)("DebuggerStatement", node, opts);
}

function isDoWhileStatement(node, opts) {
  return (0, _is.default)("DoWhileStatement", node, opts);
}

function isEmptyStatement(node, opts) {
  return (0, _is.default)("EmptyStatement", node, opts);
}

function isExpressionStatement(node, opts) {
  return (0, _is.default)("ExpressionStatement", node, opts);
}

function isFile(node, opts) {
  return (0, _is.default)("File", node, opts);
}

function isForInStatement(node, opts) {
  return (0, _is.default)("ForInStatement", node, opts);
}

function isForStatement(node, opts) {
  return (0, _is.default)("ForStatement", node, opts);
}

function isFunctionDeclaration(node, opts) {
  return (0, _is.default)("FunctionDeclaration", node, opts);
}

function isFunctionExpression(node, opts) {
  return (0, _is.default)("FunctionExpression", node, opts);
}

function isIdentifier(node, opts) {
  return (0, _is.default)("Identifier", node, opts);
}

function isIfStatement(node, opts) {
  return (0, _is.default)("IfStatement", node, opts);
}

function isLabeledStatement(node, opts) {
  return (0, _is.default)("LabeledStatement", node, opts);
}

function isStringLiteral(node, opts) {
  return (0, _is.default)("StringLiteral", node, opts);
}

function isNumericLiteral(node, opts) {
  return (0, _is.default)("NumericLiteral", node, opts);
}

function isNullLiteral(node, opts) {
  return (0, _is.default)("NullLiteral", node, opts);
}

function isBooleanLiteral(node, opts) {
  return (0, _is.default)("BooleanLiteral", node, opts);
}

function isRegExpLiteral(node, opts) {
  return (0, _is.default)("RegExpLiteral", node, opts);
}

function isLogicalExpression(node, opts) {
  return (0, _is.default)("LogicalExpression", node, opts);
}

function isMemberExpression(node, opts) {
  return (0, _is.default)("MemberExpression", node, opts);
}

function isNewExpression(node, opts) {
  return (0, _is.default)("NewExpression", node, opts);
}

function isProgram(node, opts) {
  return (0, _is.default)("Program", node, opts);
}

function isObjectExpression(node, opts) {
  return (0, _is.default)("ObjectExpression", node, opts);
}

function isObjectMethod(node, opts) {
  return (0, _is.default)("ObjectMethod", node, opts);
}

function isObjectProperty(node, opts) {
  return (0, _is.default)("ObjectProperty", node, opts);
}

function isRestElement(node, opts) {
  return (0, _is.default)("RestElement", node, opts);
}

function isReturnStatement(node, opts) {
  return (0, _is.default)("ReturnStatement", node, opts);
}

function isSequenceExpression(node, opts) {
  return (0, _is.default)("SequenceExpression", node, opts);
}

function isSwitchCase(node, opts) {
  return (0, _is.default)("SwitchCase", node, opts);
}

function isSwitchStatement(node, opts) {
  return (0, _is.default)("SwitchStatement", node, opts);
}

function isThisExpression(node, opts) {
  return (0, _is.default)("ThisExpression", node, opts);
}

function isThrowStatement(node, opts) {
  return (0, _is.default)("ThrowStatement", node, opts);
}

function isTryStatement(node, opts) {
  return (0, _is.default)("TryStatement", node, opts);
}

function isUnaryExpression(node, opts) {
  return (0, _is.default)("UnaryExpression", node, opts);
}

function isUpdateExpression(node, opts) {
  return (0, _is.default)("UpdateExpression", node, opts);
}

function isVariableDeclaration(node, opts) {
  return (0, _is.default)("VariableDeclaration", node, opts);
}

function isVariableDeclarator(node, opts) {
  return (0, _is.default)("VariableDeclarator", node, opts);
}

function isWhileStatement(node, opts) {
  return (0, _is.default)("WhileStatement", node, opts);
}

function isWithStatement(node, opts) {
  return (0, _is.default)("WithStatement", node, opts);
}

function isAssignmentPattern(node, opts) {
  return (0, _is.default)("AssignmentPattern", node, opts);
}

function isArrayPattern(node, opts) {
  return (0, _is.default)("ArrayPattern", node, opts);
}

function isArrowFunctionExpression(node, opts) {
  return (0, _is.default)("ArrowFunctionExpression", node, opts);
}

function isClassBody(node, opts) {
  return (0, _is.default)("ClassBody", node, opts);
}

function isClassDeclaration(node, opts) {
  return (0, _is.default)("ClassDeclaration", node, opts);
}

function isClassExpression(node, opts) {
  return (0, _is.default)("ClassExpression", node, opts);
}

function isExportAllDeclaration(node, opts) {
  return (0, _is.default)("ExportAllDeclaration", node, opts);
}

function isExportDefaultDeclaration(node, opts) {
  return (0, _is.default)("ExportDefaultDeclaration", node, opts);
}

function isExportNamedDeclaration(node, opts) {
  return (0, _is.default)("ExportNamedDeclaration", node, opts);
}

function isExportSpecifier(node, opts) {
  return (0, _is.default)("ExportSpecifier", node, opts);
}

function isForOfStatement(node, opts) {
  return (0, _is.default)("ForOfStatement", node, opts);
}

function isImportDeclaration(node, opts) {
  return (0, _is.default)("ImportDeclaration", node, opts);
}

function isImportDefaultSpecifier(node, opts) {
  return (0, _is.default)("ImportDefaultSpecifier", node, opts);
}

function isImportNamespaceSpecifier(node, opts) {
  return (0, _is.default)("ImportNamespaceSpecifier", node, opts);
}

function isImportSpecifier(node, opts) {
  return (0, _is.default)("ImportSpecifier", node, opts);
}

function isMetaProperty(node, opts) {
  return (0, _is.default)("MetaProperty", node, opts);
}

function isClassMethod(node, opts) {
  return (0, _is.default)("ClassMethod", node, opts);
}

function isObjectPattern(node, opts) {
  return (0, _is.default)("ObjectPattern", node, opts);
}

function isSpreadElement(node, opts) {
  return (0, _is.default)("SpreadElement", node, opts);
}

function isSuper(node, opts) {
  return (0, _is.default)("Super", node, opts);
}

function isTaggedTemplateExpression(node, opts) {
  return (0, _is.default)("TaggedTemplateExpression", node, opts);
}

function isTemplateElement(node, opts) {
  return (0, _is.default)("TemplateElement", node, opts);
}

function isTemplateLiteral(node, opts) {
  return (0, _is.default)("TemplateLiteral", node, opts);
}

function isYieldExpression(node, opts) {
  return (0, _is.default)("YieldExpression", node, opts);
}

function isAnyTypeAnnotation(node, opts) {
  return (0, _is.default)("AnyTypeAnnotation", node, opts);
}

function isArrayTypeAnnotation(node, opts) {
  return (0, _is.default)("ArrayTypeAnnotation", node, opts);
}

function isBooleanTypeAnnotation(node, opts) {
  return (0, _is.default)("BooleanTypeAnnotation", node, opts);
}

function isBooleanLiteralTypeAnnotation(node, opts) {
  return (0, _is.default)("BooleanLiteralTypeAnnotation", node, opts);
}

function isNullLiteralTypeAnnotation(node, opts) {
  return (0, _is.default)("NullLiteralTypeAnnotation", node, opts);
}

function isClassImplements(node, opts) {
  return (0, _is.default)("ClassImplements", node, opts);
}

function isDeclareClass(node, opts) {
  return (0, _is.default)("DeclareClass", node, opts);
}

function isDeclareFunction(node, opts) {
  return (0, _is.default)("DeclareFunction", node, opts);
}

function isDeclareInterface(node, opts) {
  return (0, _is.default)("DeclareInterface", node, opts);
}

function isDeclareModule(node, opts) {
  return (0, _is.default)("DeclareModule", node, opts);
}

function isDeclareModuleExports(node, opts) {
  return (0, _is.default)("DeclareModuleExports", node, opts);
}

function isDeclareTypeAlias(node, opts) {
  return (0, _is.default)("DeclareTypeAlias", node, opts);
}

function isDeclareOpaqueType(node, opts) {
  return (0, _is.default)("DeclareOpaqueType", node, opts);
}

function isDeclareVariable(node, opts) {
  return (0, _is.default)("DeclareVariable", node, opts);
}

function isDeclareExportDeclaration(node, opts) {
  return (0, _is.default)("DeclareExportDeclaration", node, opts);
}

function isDeclareExportAllDeclaration(node, opts) {
  return (0, _is.default)("DeclareExportAllDeclaration", node, opts);
}

function isDeclaredPredicate(node, opts) {
  return (0, _is.default)("DeclaredPredicate", node, opts);
}

function isExistsTypeAnnotation(node, opts) {
  return (0, _is.default)("ExistsTypeAnnotation", node, opts);
}

function isFunctionTypeAnnotation(node, opts) {
  return (0, _is.default)("FunctionTypeAnnotation", node, opts);
}

function isFunctionTypeParam(node, opts) {
  return (0, _is.default)("FunctionTypeParam", node, opts);
}

function isGenericTypeAnnotation(node, opts) {
  return (0, _is.default)("GenericTypeAnnotation", node, opts);
}

function isInferredPredicate(node, opts) {
  return (0, _is.default)("InferredPredicate", node, opts);
}

function isInterfaceExtends(node, opts) {
  return (0, _is.default)("InterfaceExtends", node, opts);
}

function isInterfaceDeclaration(node, opts) {
  return (0, _is.default)("InterfaceDeclaration", node, opts);
}

function isIntersectionTypeAnnotation(node, opts) {
  return (0, _is.default)("IntersectionTypeAnnotation", node, opts);
}

function isMixedTypeAnnotation(node, opts) {
  return (0, _is.default)("MixedTypeAnnotation", node, opts);
}

function isEmptyTypeAnnotation(node, opts) {
  return (0, _is.default)("EmptyTypeAnnotation", node, opts);
}

function isNullableTypeAnnotation(node, opts) {
  return (0, _is.default)("NullableTypeAnnotation", node, opts);
}

function isNumberLiteralTypeAnnotation(node, opts) {
  return (0, _is.default)("NumberLiteralTypeAnnotation", node, opts);
}

function isNumberTypeAnnotation(node, opts) {
  return (0, _is.default)("NumberTypeAnnotation", node, opts);
}

function isObjectTypeAnnotation(node, opts) {
  return (0, _is.default)("ObjectTypeAnnotation", node, opts);
}

function isObjectTypeCallProperty(node, opts) {
  return (0, _is.default)("ObjectTypeCallProperty", node, opts);
}

function isObjectTypeIndexer(node, opts) {
  return (0, _is.default)("ObjectTypeIndexer", node, opts);
}

function isObjectTypeProperty(node, opts) {
  return (0, _is.default)("ObjectTypeProperty", node, opts);
}

function isObjectTypeSpreadProperty(node, opts) {
  return (0, _is.default)("ObjectTypeSpreadProperty", node, opts);
}

function isOpaqueType(node, opts) {
  return (0, _is.default)("OpaqueType", node, opts);
}

function isQualifiedTypeIdentifier(node, opts) {
  return (0, _is.default)("QualifiedTypeIdentifier", node, opts);
}

function isStringLiteralTypeAnnotation(node, opts) {
  return (0, _is.default)("StringLiteralTypeAnnotation", node, opts);
}

function isStringTypeAnnotation(node, opts) {
  return (0, _is.default)("StringTypeAnnotation", node, opts);
}

function isThisTypeAnnotation(node, opts) {
  return (0, _is.default)("ThisTypeAnnotation", node, opts);
}

function isTupleTypeAnnotation(node, opts) {
  return (0, _is.default)("TupleTypeAnnotation", node, opts);
}

function isTypeofTypeAnnotation(node, opts) {
  return (0, _is.default)("TypeofTypeAnnotation", node, opts);
}

function isTypeAlias(node, opts) {
  return (0, _is.default)("TypeAlias", node, opts);
}

function isTypeAnnotation(node, opts) {
  return (0, _is.default)("TypeAnnotation", node, opts);
}

function isTypeCastExpression(node, opts) {
  return (0, _is.default)("TypeCastExpression", node, opts);
}

function isTypeParameter(node, opts) {
  return (0, _is.default)("TypeParameter", node, opts);
}

function isTypeParameterDeclaration(node, opts) {
  return (0, _is.default)("TypeParameterDeclaration", node, opts);
}

function isTypeParameterInstantiation(node, opts) {
  return (0, _is.default)("TypeParameterInstantiation", node, opts);
}

function isUnionTypeAnnotation(node, opts) {
  return (0, _is.default)("UnionTypeAnnotation", node, opts);
}

function isVoidTypeAnnotation(node, opts) {
  return (0, _is.default)("VoidTypeAnnotation", node, opts);
}

function isJSXAttribute(node, opts) {
  return (0, _is.default)("JSXAttribute", node, opts);
}

function isJSXClosingElement(node, opts) {
  return (0, _is.default)("JSXClosingElement", node, opts);
}

function isJSXElement(node, opts) {
  return (0, _is.default)("JSXElement", node, opts);
}

function isJSXEmptyExpression(node, opts) {
  return (0, _is.default)("JSXEmptyExpression", node, opts);
}

function isJSXExpressionContainer(node, opts) {
  return (0, _is.default)("JSXExpressionContainer", node, opts);
}

function isJSXSpreadChild(node, opts) {
  return (0, _is.default)("JSXSpreadChild", node, opts);
}

function isJSXIdentifier(node, opts) {
  return (0, _is.default)("JSXIdentifier", node, opts);
}

function isJSXMemberExpression(node, opts) {
  return (0, _is.default)("JSXMemberExpression", node, opts);
}

function isJSXNamespacedName(node, opts) {
  return (0, _is.default)("JSXNamespacedName", node, opts);
}

function isJSXOpeningElement(node, opts) {
  return (0, _is.default)("JSXOpeningElement", node, opts);
}

function isJSXSpreadAttribute(node, opts) {
  return (0, _is.default)("JSXSpreadAttribute", node, opts);
}

function isJSXText(node, opts) {
  return (0, _is.default)("JSXText", node, opts);
}

function isJSXFragment(node, opts) {
  return (0, _is.default)("JSXFragment", node, opts);
}

function isJSXOpeningFragment(node, opts) {
  return (0, _is.default)("JSXOpeningFragment", node, opts);
}

function isJSXClosingFragment(node, opts) {
  return (0, _is.default)("JSXClosingFragment", node, opts);
}

function isNoop(node, opts) {
  return (0, _is.default)("Noop", node, opts);
}

function isParenthesizedExpression(node, opts) {
  return (0, _is.default)("ParenthesizedExpression", node, opts);
}

function isAwaitExpression(node, opts) {
  return (0, _is.default)("AwaitExpression", node, opts);
}

function isBindExpression(node, opts) {
  return (0, _is.default)("BindExpression", node, opts);
}

function isClassProperty(node, opts) {
  return (0, _is.default)("ClassProperty", node, opts);
}

function isImport(node, opts) {
  return (0, _is.default)("Import", node, opts);
}

function isDecorator(node, opts) {
  return (0, _is.default)("Decorator", node, opts);
}

function isDoExpression(node, opts) {
  return (0, _is.default)("DoExpression", node, opts);
}

function isExportDefaultSpecifier(node, opts) {
  return (0, _is.default)("ExportDefaultSpecifier", node, opts);
}

function isExportNamespaceSpecifier(node, opts) {
  return (0, _is.default)("ExportNamespaceSpecifier", node, opts);
}

function isTSParameterProperty(node, opts) {
  return (0, _is.default)("TSParameterProperty", node, opts);
}

function isTSDeclareFunction(node, opts) {
  return (0, _is.default)("TSDeclareFunction", node, opts);
}

function isTSDeclareMethod(node, opts) {
  return (0, _is.default)("TSDeclareMethod", node, opts);
}

function isTSQualifiedName(node, opts) {
  return (0, _is.default)("TSQualifiedName", node, opts);
}

function isTSCallSignatureDeclaration(node, opts) {
  return (0, _is.default)("TSCallSignatureDeclaration", node, opts);
}

function isTSConstructSignatureDeclaration(node, opts) {
  return (0, _is.default)("TSConstructSignatureDeclaration", node, opts);
}

function isTSPropertySignature(node, opts) {
  return (0, _is.default)("TSPropertySignature", node, opts);
}

function isTSMethodSignature(node, opts) {
  return (0, _is.default)("TSMethodSignature", node, opts);
}

function isTSIndexSignature(node, opts) {
  return (0, _is.default)("TSIndexSignature", node, opts);
}

function isTSAnyKeyword(node, opts) {
  return (0, _is.default)("TSAnyKeyword", node, opts);
}

function isTSNumberKeyword(node, opts) {
  return (0, _is.default)("TSNumberKeyword", node, opts);
}

function isTSObjectKeyword(node, opts) {
  return (0, _is.default)("TSObjectKeyword", node, opts);
}

function isTSBooleanKeyword(node, opts) {
  return (0, _is.default)("TSBooleanKeyword", node, opts);
}

function isTSStringKeyword(node, opts) {
  return (0, _is.default)("TSStringKeyword", node, opts);
}

function isTSSymbolKeyword(node, opts) {
  return (0, _is.default)("TSSymbolKeyword", node, opts);
}

function isTSVoidKeyword(node, opts) {
  return (0, _is.default)("TSVoidKeyword", node, opts);
}

function isTSUndefinedKeyword(node, opts) {
  return (0, _is.default)("TSUndefinedKeyword", node, opts);
}

function isTSNullKeyword(node, opts) {
  return (0, _is.default)("TSNullKeyword", node, opts);
}

function isTSNeverKeyword(node, opts) {
  return (0, _is.default)("TSNeverKeyword", node, opts);
}

function isTSThisType(node, opts) {
  return (0, _is.default)("TSThisType", node, opts);
}

function isTSFunctionType(node, opts) {
  return (0, _is.default)("TSFunctionType", node, opts);
}

function isTSConstructorType(node, opts) {
  return (0, _is.default)("TSConstructorType", node, opts);
}

function isTSTypeReference(node, opts) {
  return (0, _is.default)("TSTypeReference", node, opts);
}

function isTSTypePredicate(node, opts) {
  return (0, _is.default)("TSTypePredicate", node, opts);
}

function isTSTypeQuery(node, opts) {
  return (0, _is.default)("TSTypeQuery", node, opts);
}

function isTSTypeLiteral(node, opts) {
  return (0, _is.default)("TSTypeLiteral", node, opts);
}

function isTSArrayType(node, opts) {
  return (0, _is.default)("TSArrayType", node, opts);
}

function isTSTupleType(node, opts) {
  return (0, _is.default)("TSTupleType", node, opts);
}

function isTSUnionType(node, opts) {
  return (0, _is.default)("TSUnionType", node, opts);
}

function isTSIntersectionType(node, opts) {
  return (0, _is.default)("TSIntersectionType", node, opts);
}

function isTSParenthesizedType(node, opts) {
  return (0, _is.default)("TSParenthesizedType", node, opts);
}

function isTSTypeOperator(node, opts) {
  return (0, _is.default)("TSTypeOperator", node, opts);
}

function isTSIndexedAccessType(node, opts) {
  return (0, _is.default)("TSIndexedAccessType", node, opts);
}

function isTSMappedType(node, opts) {
  return (0, _is.default)("TSMappedType", node, opts);
}

function isTSLiteralType(node, opts) {
  return (0, _is.default)("TSLiteralType", node, opts);
}

function isTSExpressionWithTypeArguments(node, opts) {
  return (0, _is.default)("TSExpressionWithTypeArguments", node, opts);
}

function isTSInterfaceDeclaration(node, opts) {
  return (0, _is.default)("TSInterfaceDeclaration", node, opts);
}

function isTSInterfaceBody(node, opts) {
  return (0, _is.default)("TSInterfaceBody", node, opts);
}

function isTSTypeAliasDeclaration(node, opts) {
  return (0, _is.default)("TSTypeAliasDeclaration", node, opts);
}

function isTSAsExpression(node, opts) {
  return (0, _is.default)("TSAsExpression", node, opts);
}

function isTSTypeAssertion(node, opts) {
  return (0, _is.default)("TSTypeAssertion", node, opts);
}

function isTSEnumDeclaration(node, opts) {
  return (0, _is.default)("TSEnumDeclaration", node, opts);
}

function isTSEnumMember(node, opts) {
  return (0, _is.default)("TSEnumMember", node, opts);
}

function isTSModuleDeclaration(node, opts) {
  return (0, _is.default)("TSModuleDeclaration", node, opts);
}

function isTSModuleBlock(node, opts) {
  return (0, _is.default)("TSModuleBlock", node, opts);
}

function isTSImportEqualsDeclaration(node, opts) {
  return (0, _is.default)("TSImportEqualsDeclaration", node, opts);
}

function isTSExternalModuleReference(node, opts) {
  return (0, _is.default)("TSExternalModuleReference", node, opts);
}

function isTSNonNullExpression(node, opts) {
  return (0, _is.default)("TSNonNullExpression", node, opts);
}

function isTSExportAssignment(node, opts) {
  return (0, _is.default)("TSExportAssignment", node, opts);
}

function isTSNamespaceExportDeclaration(node, opts) {
  return (0, _is.default)("TSNamespaceExportDeclaration", node, opts);
}

function isTSTypeAnnotation(node, opts) {
  return (0, _is.default)("TSTypeAnnotation", node, opts);
}

function isTSTypeParameterInstantiation(node, opts) {
  return (0, _is.default)("TSTypeParameterInstantiation", node, opts);
}

function isTSTypeParameterDeclaration(node, opts) {
  return (0, _is.default)("TSTypeParameterDeclaration", node, opts);
}

function isTSTypeParameter(node, opts) {
  return (0, _is.default)("TSTypeParameter", node, opts);
}

function isExpression(node, opts) {
  return (0, _is.default)("Expression", node, opts);
}

function isBinary(node, opts) {
  return (0, _is.default)("Binary", node, opts);
}

function isScopable(node, opts) {
  return (0, _is.default)("Scopable", node, opts);
}

function isBlockParent(node, opts) {
  return (0, _is.default)("BlockParent", node, opts);
}

function isBlock(node, opts) {
  return (0, _is.default)("Block", node, opts);
}

function isStatement(node, opts) {
  return (0, _is.default)("Statement", node, opts);
}

function isTerminatorless(node, opts) {
  return (0, _is.default)("Terminatorless", node, opts);
}

function isCompletionStatement(node, opts) {
  return (0, _is.default)("CompletionStatement", node, opts);
}

function isConditional(node, opts) {
  return (0, _is.default)("Conditional", node, opts);
}

function isLoop(node, opts) {
  return (0, _is.default)("Loop", node, opts);
}

function isWhile(node, opts) {
  return (0, _is.default)("While", node, opts);
}

function isExpressionWrapper(node, opts) {
  return (0, _is.default)("ExpressionWrapper", node, opts);
}

function isFor(node, opts) {
  return (0, _is.default)("For", node, opts);
}

function isForXStatement(node, opts) {
  return (0, _is.default)("ForXStatement", node, opts);
}

function isFunction(node, opts) {
  return (0, _is.default)("Function", node, opts);
}

function isFunctionParent(node, opts) {
  return (0, _is.default)("FunctionParent", node, opts);
}

function isPureish(node, opts) {
  return (0, _is.default)("Pureish", node, opts);
}

function isDeclaration(node, opts) {
  return (0, _is.default)("Declaration", node, opts);
}

function isPatternLike(node, opts) {
  return (0, _is.default)("PatternLike", node, opts);
}

function isLVal(node, opts) {
  return (0, _is.default)("LVal", node, opts);
}

function isTSEntityName(node, opts) {
  return (0, _is.default)("TSEntityName", node, opts);
}

function isLiteral(node, opts) {
  return (0, _is.default)("Literal", node, opts);
}

function isImmutable(node, opts) {
  return (0, _is.default)("Immutable", node, opts);
}

function isUserWhitespacable(node, opts) {
  return (0, _is.default)("UserWhitespacable", node, opts);
}

function isMethod(node, opts) {
  return (0, _is.default)("Method", node, opts);
}

function isObjectMember(node, opts) {
  return (0, _is.default)("ObjectMember", node, opts);
}

function isProperty(node, opts) {
  return (0, _is.default)("Property", node, opts);
}

function isUnaryLike(node, opts) {
  return (0, _is.default)("UnaryLike", node, opts);
}

function isPattern(node, opts) {
  return (0, _is.default)("Pattern", node, opts);
}

function isClass(node, opts) {
  return (0, _is.default)("Class", node, opts);
}

function isModuleDeclaration(node, opts) {
  return (0, _is.default)("ModuleDeclaration", node, opts);
}

function isExportDeclaration(node, opts) {
  return (0, _is.default)("ExportDeclaration", node, opts);
}

function isModuleSpecifier(node, opts) {
  return (0, _is.default)("ModuleSpecifier", node, opts);
}

function isFlow(node, opts) {
  return (0, _is.default)("Flow", node, opts);
}

function isFlowBaseAnnotation(node, opts) {
  return (0, _is.default)("FlowBaseAnnotation", node, opts);
}

function isFlowDeclaration(node, opts) {
  return (0, _is.default)("FlowDeclaration", node, opts);
}

function isFlowPredicate(node, opts) {
  return (0, _is.default)("FlowPredicate", node, opts);
}

function isJSX(node, opts) {
  return (0, _is.default)("JSX", node, opts);
}

function isTSTypeElement(node, opts) {
  return (0, _is.default)("TSTypeElement", node, opts);
}

function isTSType(node, opts) {
  return (0, _is.default)("TSType", node, opts);
}

function isNumberLiteral(node, opts) {
  console.trace("The node type NumberLiteral has been renamed to NumericLiteral");
  return (0, _is.default)("NumberLiteral", node, opts);
}

function isRegexLiteral(node, opts) {
  console.trace("The node type RegexLiteral has been renamed to RegExpLiteral");
  return (0, _is.default)("RegexLiteral", node, opts);
}

function isRestProperty(node, opts) {
  console.trace("The node type RestProperty has been renamed to RestElement");
  return (0, _is.default)("RestProperty", node, opts);
}

function isSpreadProperty(node, opts) {
  console.trace("The node type SpreadProperty has been renamed to SpreadElement");
  return (0, _is.default)("SpreadProperty", node, opts);
}