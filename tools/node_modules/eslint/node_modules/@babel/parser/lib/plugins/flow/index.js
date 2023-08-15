"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;
var _types = require("../../tokenizer/types");
var _context = require("../../tokenizer/context");
var _identifier = require("../../util/identifier");
var _scope = require("./scope");
var _scopeflags = require("../../util/scopeflags");
var _parseError = require("../../parse-error");
var _node = require("../../parser/node");
const reservedTypes = new Set(["_", "any", "bool", "boolean", "empty", "extends", "false", "interface", "mixed", "null", "number", "static", "string", "true", "typeof", "void"]);
const FlowErrors = (0, _parseError.ParseErrorEnum)`flow`({
  AmbiguousConditionalArrow: "Ambiguous expression: wrap the arrow functions in parentheses to disambiguate.",
  AmbiguousDeclareModuleKind: "Found both `declare module.exports` and `declare export` in the same module. Modules can only have 1 since they are either an ES module or they are a CommonJS module.",
  AssignReservedType: ({
    reservedType
  }) => `Cannot overwrite reserved type ${reservedType}.`,
  DeclareClassElement: "The `declare` modifier can only appear on class fields.",
  DeclareClassFieldInitializer: "Initializers are not allowed in fields with the `declare` modifier.",
  DuplicateDeclareModuleExports: "Duplicate `declare module.exports` statement.",
  EnumBooleanMemberNotInitialized: ({
    memberName,
    enumName
  }) => `Boolean enum members need to be initialized. Use either \`${memberName} = true,\` or \`${memberName} = false,\` in enum \`${enumName}\`.`,
  EnumDuplicateMemberName: ({
    memberName,
    enumName
  }) => `Enum member names need to be unique, but the name \`${memberName}\` has already been used before in enum \`${enumName}\`.`,
  EnumInconsistentMemberValues: ({
    enumName
  }) => `Enum \`${enumName}\` has inconsistent member initializers. Either use no initializers, or consistently use literals (either booleans, numbers, or strings) for all member initializers.`,
  EnumInvalidExplicitType: ({
    invalidEnumType,
    enumName
  }) => `Enum type \`${invalidEnumType}\` is not valid. Use one of \`boolean\`, \`number\`, \`string\`, or \`symbol\` in enum \`${enumName}\`.`,
  EnumInvalidExplicitTypeUnknownSupplied: ({
    enumName
  }) => `Supplied enum type is not valid. Use one of \`boolean\`, \`number\`, \`string\`, or \`symbol\` in enum \`${enumName}\`.`,
  EnumInvalidMemberInitializerPrimaryType: ({
    enumName,
    memberName,
    explicitType
  }) => `Enum \`${enumName}\` has type \`${explicitType}\`, so the initializer of \`${memberName}\` needs to be a ${explicitType} literal.`,
  EnumInvalidMemberInitializerSymbolType: ({
    enumName,
    memberName
  }) => `Symbol enum members cannot be initialized. Use \`${memberName},\` in enum \`${enumName}\`.`,
  EnumInvalidMemberInitializerUnknownType: ({
    enumName,
    memberName
  }) => `The enum member initializer for \`${memberName}\` needs to be a literal (either a boolean, number, or string) in enum \`${enumName}\`.`,
  EnumInvalidMemberName: ({
    enumName,
    memberName,
    suggestion
  }) => `Enum member names cannot start with lowercase 'a' through 'z'. Instead of using \`${memberName}\`, consider using \`${suggestion}\`, in enum \`${enumName}\`.`,
  EnumNumberMemberNotInitialized: ({
    enumName,
    memberName
  }) => `Number enum members need to be initialized, e.g. \`${memberName} = 1\` in enum \`${enumName}\`.`,
  EnumStringMemberInconsistentlyInitailized: ({
    enumName
  }) => `String enum members need to consistently either all use initializers, or use no initializers, in enum \`${enumName}\`.`,
  GetterMayNotHaveThisParam: "A getter cannot have a `this` parameter.",
  ImportReflectionHasImportType: "An `import module` declaration can not use `type` or `typeof` keyword.",
  ImportTypeShorthandOnlyInPureImport: "The `type` and `typeof` keywords on named imports can only be used on regular `import` statements. It cannot be used with `import type` or `import typeof` statements.",
  InexactInsideExact: "Explicit inexact syntax cannot appear inside an explicit exact object type.",
  InexactInsideNonObject: "Explicit inexact syntax cannot appear in class or interface definitions.",
  InexactVariance: "Explicit inexact syntax cannot have variance.",
  InvalidNonTypeImportInDeclareModule: "Imports within a `declare module` body must always be `import type` or `import typeof`.",
  MissingTypeParamDefault: "Type parameter declaration needs a default, since a preceding type parameter declaration has a default.",
  NestedDeclareModule: "`declare module` cannot be used inside another `declare module`.",
  NestedFlowComment: "Cannot have a flow comment inside another flow comment.",
  PatternIsOptional: Object.assign({
    message: "A binding pattern parameter cannot be optional in an implementation signature."
  }, {
    reasonCode: "OptionalBindingPattern"
  }),
  SetterMayNotHaveThisParam: "A setter cannot have a `this` parameter.",
  SpreadVariance: "Spread properties cannot have variance.",
  ThisParamAnnotationRequired: "A type annotation is required for the `this` parameter.",
  ThisParamBannedInConstructor: "Constructors cannot have a `this` parameter; constructors don't bind `this` like other functions.",
  ThisParamMayNotBeOptional: "The `this` parameter cannot be optional.",
  ThisParamMustBeFirst: "The `this` parameter must be the first function parameter.",
  ThisParamNoDefault: "The `this` parameter may not have a default value.",
  TypeBeforeInitializer: "Type annotations must come before default assignments, e.g. instead of `age = 25: number` use `age: number = 25`.",
  TypeCastInPattern: "The type cast expression is expected to be wrapped with parenthesis.",
  UnexpectedExplicitInexactInObject: "Explicit inexact syntax must appear at the end of an inexact object.",
  UnexpectedReservedType: ({
    reservedType
  }) => `Unexpected reserved type ${reservedType}.`,
  UnexpectedReservedUnderscore: "`_` is only allowed as a type argument to call or new.",
  UnexpectedSpaceBetweenModuloChecks: "Spaces between `%` and `checks` are not allowed here.",
  UnexpectedSpreadType: "Spread operator cannot appear in class or interface definitions.",
  UnexpectedSubtractionOperand: 'Unexpected token, expected "number" or "bigint".',
  UnexpectedTokenAfterTypeParameter: "Expected an arrow function after this type parameter declaration.",
  UnexpectedTypeParameterBeforeAsyncArrowFunction: "Type parameters must come after the async keyword, e.g. instead of `<T> async () => {}`, use `async <T>() => {}`.",
  UnsupportedDeclareExportKind: ({
    unsupportedExportKind,
    suggestion
  }) => `\`declare export ${unsupportedExportKind}\` is not supported. Use \`${suggestion}\` instead.`,
  UnsupportedStatementInDeclareModule: "Only declares and type imports are allowed inside declare module.",
  UnterminatedFlowComment: "Unterminated flow-comment."
});
function isEsModuleType(bodyElement) {
  return bodyElement.type === "DeclareExportAllDeclaration" || bodyElement.type === "DeclareExportDeclaration" && (!bodyElement.declaration || bodyElement.declaration.type !== "TypeAlias" && bodyElement.declaration.type !== "InterfaceDeclaration");
}
function hasTypeImportKind(node) {
  return node.importKind === "type" || node.importKind === "typeof";
}
const exportSuggestions = {
  const: "declare export var",
  let: "declare export var",
  type: "export type",
  interface: "export interface"
};
function partition(list, test) {
  const list1 = [];
  const list2 = [];
  for (let i = 0; i < list.length; i++) {
    (test(list[i], i, list) ? list1 : list2).push(list[i]);
  }
  return [list1, list2];
}
const FLOW_PRAGMA_REGEX = /\*?\s*@((?:no)?flow)\b/;
var _default = superClass => class FlowParserMixin extends superClass {
  constructor(...args) {
    super(...args);
    this.flowPragma = undefined;
  }
  getScopeHandler() {
    return _scope.default;
  }
  shouldParseTypes() {
    return this.getPluginOption("flow", "all") || this.flowPragma === "flow";
  }
  shouldParseEnums() {
    return !!this.getPluginOption("flow", "enums");
  }
  finishToken(type, val) {
    if (type !== 131 && type !== 13 && type !== 28) {
      if (this.flowPragma === undefined) {
        this.flowPragma = null;
      }
    }
    super.finishToken(type, val);
  }
  addComment(comment) {
    if (this.flowPragma === undefined) {
      const matches = FLOW_PRAGMA_REGEX.exec(comment.value);
      if (!matches) {} else if (matches[1] === "flow") {
        this.flowPragma = "flow";
      } else if (matches[1] === "noflow") {
        this.flowPragma = "noflow";
      } else {
        throw new Error("Unexpected flow pragma");
      }
    }
    super.addComment(comment);
  }
  flowParseTypeInitialiser(tok) {
    const oldInType = this.state.inType;
    this.state.inType = true;
    this.expect(tok || 14);
    const type = this.flowParseType();
    this.state.inType = oldInType;
    return type;
  }
  flowParsePredicate() {
    const node = this.startNode();
    const moduloLoc = this.state.startLoc;
    this.next();
    this.expectContextual(108);
    if (this.state.lastTokStart > moduloLoc.index + 1) {
      this.raise(FlowErrors.UnexpectedSpaceBetweenModuloChecks, {
        at: moduloLoc
      });
    }
    if (this.eat(10)) {
      node.value = super.parseExpression();
      this.expect(11);
      return this.finishNode(node, "DeclaredPredicate");
    } else {
      return this.finishNode(node, "InferredPredicate");
    }
  }
  flowParseTypeAndPredicateInitialiser() {
    const oldInType = this.state.inType;
    this.state.inType = true;
    this.expect(14);
    let type = null;
    let predicate = null;
    if (this.match(54)) {
      this.state.inType = oldInType;
      predicate = this.flowParsePredicate();
    } else {
      type = this.flowParseType();
      this.state.inType = oldInType;
      if (this.match(54)) {
        predicate = this.flowParsePredicate();
      }
    }
    return [type, predicate];
  }
  flowParseDeclareClass(node) {
    this.next();
    this.flowParseInterfaceish(node, true);
    return this.finishNode(node, "DeclareClass");
  }
  flowParseDeclareFunction(node) {
    this.next();
    const id = node.id = this.parseIdentifier();
    const typeNode = this.startNode();
    const typeContainer = this.startNode();
    if (this.match(47)) {
      typeNode.typeParameters = this.flowParseTypeParameterDeclaration();
    } else {
      typeNode.typeParameters = null;
    }
    this.expect(10);
    const tmp = this.flowParseFunctionTypeParams();
    typeNode.params = tmp.params;
    typeNode.rest = tmp.rest;
    typeNode.this = tmp._this;
    this.expect(11);
    [typeNode.returnType, node.predicate] = this.flowParseTypeAndPredicateInitialiser();
    typeContainer.typeAnnotation = this.finishNode(typeNode, "FunctionTypeAnnotation");
    id.typeAnnotation = this.finishNode(typeContainer, "TypeAnnotation");
    this.resetEndLocation(id);
    this.semicolon();
    this.scope.declareName(node.id.name, _scopeflags.BindingFlag.TYPE_FLOW_DECLARE_FN, node.id.loc.start);
    return this.finishNode(node, "DeclareFunction");
  }
  flowParseDeclare(node, insideModule) {
    if (this.match(80)) {
      return this.flowParseDeclareClass(node);
    } else if (this.match(68)) {
      return this.flowParseDeclareFunction(node);
    } else if (this.match(74)) {
      return this.flowParseDeclareVariable(node);
    } else if (this.eatContextual(125)) {
      if (this.match(16)) {
        return this.flowParseDeclareModuleExports(node);
      } else {
        if (insideModule) {
          this.raise(FlowErrors.NestedDeclareModule, {
            at: this.state.lastTokStartLoc
          });
        }
        return this.flowParseDeclareModule(node);
      }
    } else if (this.isContextual(128)) {
      return this.flowParseDeclareTypeAlias(node);
    } else if (this.isContextual(129)) {
      return this.flowParseDeclareOpaqueType(node);
    } else if (this.isContextual(127)) {
      return this.flowParseDeclareInterface(node);
    } else if (this.match(82)) {
      return this.flowParseDeclareExportDeclaration(node, insideModule);
    } else {
      this.unexpected();
    }
  }
  flowParseDeclareVariable(node) {
    this.next();
    node.id = this.flowParseTypeAnnotatableIdentifier(true);
    this.scope.declareName(node.id.name, _scopeflags.BindingFlag.TYPE_VAR, node.id.loc.start);
    this.semicolon();
    return this.finishNode(node, "DeclareVariable");
  }
  flowParseDeclareModule(node) {
    this.scope.enter(_scopeflags.ScopeFlag.OTHER);
    if (this.match(131)) {
      node.id = super.parseExprAtom();
    } else {
      node.id = this.parseIdentifier();
    }
    const bodyNode = node.body = this.startNode();
    const body = bodyNode.body = [];
    this.expect(5);
    while (!this.match(8)) {
      let bodyNode = this.startNode();
      if (this.match(83)) {
        this.next();
        if (!this.isContextual(128) && !this.match(87)) {
          this.raise(FlowErrors.InvalidNonTypeImportInDeclareModule, {
            at: this.state.lastTokStartLoc
          });
        }
        super.parseImport(bodyNode);
      } else {
        this.expectContextual(123, FlowErrors.UnsupportedStatementInDeclareModule);
        bodyNode = this.flowParseDeclare(bodyNode, true);
      }
      body.push(bodyNode);
    }
    this.scope.exit();
    this.expect(8);
    this.finishNode(bodyNode, "BlockStatement");
    let kind = null;
    let hasModuleExport = false;
    body.forEach(bodyElement => {
      if (isEsModuleType(bodyElement)) {
        if (kind === "CommonJS") {
          this.raise(FlowErrors.AmbiguousDeclareModuleKind, {
            at: bodyElement
          });
        }
        kind = "ES";
      } else if (bodyElement.type === "DeclareModuleExports") {
        if (hasModuleExport) {
          this.raise(FlowErrors.DuplicateDeclareModuleExports, {
            at: bodyElement
          });
        }
        if (kind === "ES") {
          this.raise(FlowErrors.AmbiguousDeclareModuleKind, {
            at: bodyElement
          });
        }
        kind = "CommonJS";
        hasModuleExport = true;
      }
    });
    node.kind = kind || "CommonJS";
    return this.finishNode(node, "DeclareModule");
  }
  flowParseDeclareExportDeclaration(node, insideModule) {
    this.expect(82);
    if (this.eat(65)) {
      if (this.match(68) || this.match(80)) {
        node.declaration = this.flowParseDeclare(this.startNode());
      } else {
        node.declaration = this.flowParseType();
        this.semicolon();
      }
      node.default = true;
      return this.finishNode(node, "DeclareExportDeclaration");
    } else {
      if (this.match(75) || this.isLet() || (this.isContextual(128) || this.isContextual(127)) && !insideModule) {
        const label = this.state.value;
        throw this.raise(FlowErrors.UnsupportedDeclareExportKind, {
          at: this.state.startLoc,
          unsupportedExportKind: label,
          suggestion: exportSuggestions[label]
        });
      }
      if (this.match(74) || this.match(68) || this.match(80) || this.isContextual(129)) {
        node.declaration = this.flowParseDeclare(this.startNode());
        node.default = false;
        return this.finishNode(node, "DeclareExportDeclaration");
      } else if (this.match(55) || this.match(5) || this.isContextual(127) || this.isContextual(128) || this.isContextual(129)) {
        node = this.parseExport(node, null);
        if (node.type === "ExportNamedDeclaration") {
          node.type = "ExportDeclaration";
          node.default = false;
          delete node.exportKind;
        }
        node.type = "Declare" + node.type;
        return node;
      }
    }
    this.unexpected();
  }
  flowParseDeclareModuleExports(node) {
    this.next();
    this.expectContextual(109);
    node.typeAnnotation = this.flowParseTypeAnnotation();
    this.semicolon();
    return this.finishNode(node, "DeclareModuleExports");
  }
  flowParseDeclareTypeAlias(node) {
    this.next();
    const finished = this.flowParseTypeAlias(node);
    finished.type = "DeclareTypeAlias";
    return finished;
  }
  flowParseDeclareOpaqueType(node) {
    this.next();
    const finished = this.flowParseOpaqueType(node, true);
    finished.type = "DeclareOpaqueType";
    return finished;
  }
  flowParseDeclareInterface(node) {
    this.next();
    this.flowParseInterfaceish(node, false);
    return this.finishNode(node, "DeclareInterface");
  }
  flowParseInterfaceish(node, isClass) {
    node.id = this.flowParseRestrictedIdentifier(!isClass, true);
    this.scope.declareName(node.id.name, isClass ? _scopeflags.BindingFlag.TYPE_FUNCTION : _scopeflags.BindingFlag.TYPE_LEXICAL, node.id.loc.start);
    if (this.match(47)) {
      node.typeParameters = this.flowParseTypeParameterDeclaration();
    } else {
      node.typeParameters = null;
    }
    node.extends = [];
    if (this.eat(81)) {
      do {
        node.extends.push(this.flowParseInterfaceExtends());
      } while (!isClass && this.eat(12));
    }
    if (isClass) {
      node.implements = [];
      node.mixins = [];
      if (this.eatContextual(115)) {
        do {
          node.mixins.push(this.flowParseInterfaceExtends());
        } while (this.eat(12));
      }
      if (this.eatContextual(111)) {
        do {
          node.implements.push(this.flowParseInterfaceExtends());
        } while (this.eat(12));
      }
    }
    node.body = this.flowParseObjectType({
      allowStatic: isClass,
      allowExact: false,
      allowSpread: false,
      allowProto: isClass,
      allowInexact: false
    });
  }
  flowParseInterfaceExtends() {
    const node = this.startNode();
    node.id = this.flowParseQualifiedTypeIdentifier();
    if (this.match(47)) {
      node.typeParameters = this.flowParseTypeParameterInstantiation();
    } else {
      node.typeParameters = null;
    }
    return this.finishNode(node, "InterfaceExtends");
  }
  flowParseInterface(node) {
    this.flowParseInterfaceish(node, false);
    return this.finishNode(node, "InterfaceDeclaration");
  }
  checkNotUnderscore(word) {
    if (word === "_") {
      this.raise(FlowErrors.UnexpectedReservedUnderscore, {
        at: this.state.startLoc
      });
    }
  }
  checkReservedType(word, startLoc, declaration) {
    if (!reservedTypes.has(word)) return;
    this.raise(declaration ? FlowErrors.AssignReservedType : FlowErrors.UnexpectedReservedType, {
      at: startLoc,
      reservedType: word
    });
  }
  flowParseRestrictedIdentifier(liberal, declaration) {
    this.checkReservedType(this.state.value, this.state.startLoc, declaration);
    return this.parseIdentifier(liberal);
  }
  flowParseTypeAlias(node) {
    node.id = this.flowParseRestrictedIdentifier(false, true);
    this.scope.declareName(node.id.name, _scopeflags.BindingFlag.TYPE_LEXICAL, node.id.loc.start);
    if (this.match(47)) {
      node.typeParameters = this.flowParseTypeParameterDeclaration();
    } else {
      node.typeParameters = null;
    }
    node.right = this.flowParseTypeInitialiser(29);
    this.semicolon();
    return this.finishNode(node, "TypeAlias");
  }
  flowParseOpaqueType(node, declare) {
    this.expectContextual(128);
    node.id = this.flowParseRestrictedIdentifier(true, true);
    this.scope.declareName(node.id.name, _scopeflags.BindingFlag.TYPE_LEXICAL, node.id.loc.start);
    if (this.match(47)) {
      node.typeParameters = this.flowParseTypeParameterDeclaration();
    } else {
      node.typeParameters = null;
    }
    node.supertype = null;
    if (this.match(14)) {
      node.supertype = this.flowParseTypeInitialiser(14);
    }
    node.impltype = null;
    if (!declare) {
      node.impltype = this.flowParseTypeInitialiser(29);
    }
    this.semicolon();
    return this.finishNode(node, "OpaqueType");
  }
  flowParseTypeParameter(requireDefault = false) {
    const nodeStartLoc = this.state.startLoc;
    const node = this.startNode();
    const variance = this.flowParseVariance();
    const ident = this.flowParseTypeAnnotatableIdentifier();
    node.name = ident.name;
    node.variance = variance;
    node.bound = ident.typeAnnotation;
    if (this.match(29)) {
      this.eat(29);
      node.default = this.flowParseType();
    } else {
      if (requireDefault) {
        this.raise(FlowErrors.MissingTypeParamDefault, {
          at: nodeStartLoc
        });
      }
    }
    return this.finishNode(node, "TypeParameter");
  }
  flowParseTypeParameterDeclaration() {
    const oldInType = this.state.inType;
    const node = this.startNode();
    node.params = [];
    this.state.inType = true;
    if (this.match(47) || this.match(140)) {
      this.next();
    } else {
      this.unexpected();
    }
    let defaultRequired = false;
    do {
      const typeParameter = this.flowParseTypeParameter(defaultRequired);
      node.params.push(typeParameter);
      if (typeParameter.default) {
        defaultRequired = true;
      }
      if (!this.match(48)) {
        this.expect(12);
      }
    } while (!this.match(48));
    this.expect(48);
    this.state.inType = oldInType;
    return this.finishNode(node, "TypeParameterDeclaration");
  }
  flowParseTypeParameterInstantiation() {
    const node = this.startNode();
    const oldInType = this.state.inType;
    node.params = [];
    this.state.inType = true;
    this.expect(47);
    const oldNoAnonFunctionType = this.state.noAnonFunctionType;
    this.state.noAnonFunctionType = false;
    while (!this.match(48)) {
      node.params.push(this.flowParseType());
      if (!this.match(48)) {
        this.expect(12);
      }
    }
    this.state.noAnonFunctionType = oldNoAnonFunctionType;
    this.expect(48);
    this.state.inType = oldInType;
    return this.finishNode(node, "TypeParameterInstantiation");
  }
  flowParseTypeParameterInstantiationCallOrNew() {
    const node = this.startNode();
    const oldInType = this.state.inType;
    node.params = [];
    this.state.inType = true;
    this.expect(47);
    while (!this.match(48)) {
      node.params.push(this.flowParseTypeOrImplicitInstantiation());
      if (!this.match(48)) {
        this.expect(12);
      }
    }
    this.expect(48);
    this.state.inType = oldInType;
    return this.finishNode(node, "TypeParameterInstantiation");
  }
  flowParseInterfaceType() {
    const node = this.startNode();
    this.expectContextual(127);
    node.extends = [];
    if (this.eat(81)) {
      do {
        node.extends.push(this.flowParseInterfaceExtends());
      } while (this.eat(12));
    }
    node.body = this.flowParseObjectType({
      allowStatic: false,
      allowExact: false,
      allowSpread: false,
      allowProto: false,
      allowInexact: false
    });
    return this.finishNode(node, "InterfaceTypeAnnotation");
  }
  flowParseObjectPropertyKey() {
    return this.match(132) || this.match(131) ? super.parseExprAtom() : this.parseIdentifier(true);
  }
  flowParseObjectTypeIndexer(node, isStatic, variance) {
    node.static = isStatic;
    if (this.lookahead().type === 14) {
      node.id = this.flowParseObjectPropertyKey();
      node.key = this.flowParseTypeInitialiser();
    } else {
      node.id = null;
      node.key = this.flowParseType();
    }
    this.expect(3);
    node.value = this.flowParseTypeInitialiser();
    node.variance = variance;
    return this.finishNode(node, "ObjectTypeIndexer");
  }
  flowParseObjectTypeInternalSlot(node, isStatic) {
    node.static = isStatic;
    node.id = this.flowParseObjectPropertyKey();
    this.expect(3);
    this.expect(3);
    if (this.match(47) || this.match(10)) {
      node.method = true;
      node.optional = false;
      node.value = this.flowParseObjectTypeMethodish(this.startNodeAt(node.loc.start));
    } else {
      node.method = false;
      if (this.eat(17)) {
        node.optional = true;
      }
      node.value = this.flowParseTypeInitialiser();
    }
    return this.finishNode(node, "ObjectTypeInternalSlot");
  }
  flowParseObjectTypeMethodish(node) {
    node.params = [];
    node.rest = null;
    node.typeParameters = null;
    node.this = null;
    if (this.match(47)) {
      node.typeParameters = this.flowParseTypeParameterDeclaration();
    }
    this.expect(10);
    if (this.match(78)) {
      node.this = this.flowParseFunctionTypeParam(true);
      node.this.name = null;
      if (!this.match(11)) {
        this.expect(12);
      }
    }
    while (!this.match(11) && !this.match(21)) {
      node.params.push(this.flowParseFunctionTypeParam(false));
      if (!this.match(11)) {
        this.expect(12);
      }
    }
    if (this.eat(21)) {
      node.rest = this.flowParseFunctionTypeParam(false);
    }
    this.expect(11);
    node.returnType = this.flowParseTypeInitialiser();
    return this.finishNode(node, "FunctionTypeAnnotation");
  }
  flowParseObjectTypeCallProperty(node, isStatic) {
    const valueNode = this.startNode();
    node.static = isStatic;
    node.value = this.flowParseObjectTypeMethodish(valueNode);
    return this.finishNode(node, "ObjectTypeCallProperty");
  }
  flowParseObjectType({
    allowStatic,
    allowExact,
    allowSpread,
    allowProto,
    allowInexact
  }) {
    const oldInType = this.state.inType;
    this.state.inType = true;
    const nodeStart = this.startNode();
    nodeStart.callProperties = [];
    nodeStart.properties = [];
    nodeStart.indexers = [];
    nodeStart.internalSlots = [];
    let endDelim;
    let exact;
    let inexact = false;
    if (allowExact && this.match(6)) {
      this.expect(6);
      endDelim = 9;
      exact = true;
    } else {
      this.expect(5);
      endDelim = 8;
      exact = false;
    }
    nodeStart.exact = exact;
    while (!this.match(endDelim)) {
      let isStatic = false;
      let protoStartLoc = null;
      let inexactStartLoc = null;
      const node = this.startNode();
      if (allowProto && this.isContextual(116)) {
        const lookahead = this.lookahead();
        if (lookahead.type !== 14 && lookahead.type !== 17) {
          this.next();
          protoStartLoc = this.state.startLoc;
          allowStatic = false;
        }
      }
      if (allowStatic && this.isContextual(104)) {
        const lookahead = this.lookahead();
        if (lookahead.type !== 14 && lookahead.type !== 17) {
          this.next();
          isStatic = true;
        }
      }
      const variance = this.flowParseVariance();
      if (this.eat(0)) {
        if (protoStartLoc != null) {
          this.unexpected(protoStartLoc);
        }
        if (this.eat(0)) {
          if (variance) {
            this.unexpected(variance.loc.start);
          }
          nodeStart.internalSlots.push(this.flowParseObjectTypeInternalSlot(node, isStatic));
        } else {
          nodeStart.indexers.push(this.flowParseObjectTypeIndexer(node, isStatic, variance));
        }
      } else if (this.match(10) || this.match(47)) {
        if (protoStartLoc != null) {
          this.unexpected(protoStartLoc);
        }
        if (variance) {
          this.unexpected(variance.loc.start);
        }
        nodeStart.callProperties.push(this.flowParseObjectTypeCallProperty(node, isStatic));
      } else {
        let kind = "init";
        if (this.isContextual(98) || this.isContextual(103)) {
          const lookahead = this.lookahead();
          if ((0, _types.tokenIsLiteralPropertyName)(lookahead.type)) {
            kind = this.state.value;
            this.next();
          }
        }
        const propOrInexact = this.flowParseObjectTypeProperty(node, isStatic, protoStartLoc, variance, kind, allowSpread, allowInexact != null ? allowInexact : !exact);
        if (propOrInexact === null) {
          inexact = true;
          inexactStartLoc = this.state.lastTokStartLoc;
        } else {
          nodeStart.properties.push(propOrInexact);
        }
      }
      this.flowObjectTypeSemicolon();
      if (inexactStartLoc && !this.match(8) && !this.match(9)) {
        this.raise(FlowErrors.UnexpectedExplicitInexactInObject, {
          at: inexactStartLoc
        });
      }
    }
    this.expect(endDelim);
    if (allowSpread) {
      nodeStart.inexact = inexact;
    }
    const out = this.finishNode(nodeStart, "ObjectTypeAnnotation");
    this.state.inType = oldInType;
    return out;
  }
  flowParseObjectTypeProperty(node, isStatic, protoStartLoc, variance, kind, allowSpread, allowInexact) {
    if (this.eat(21)) {
      const isInexactToken = this.match(12) || this.match(13) || this.match(8) || this.match(9);
      if (isInexactToken) {
        if (!allowSpread) {
          this.raise(FlowErrors.InexactInsideNonObject, {
            at: this.state.lastTokStartLoc
          });
        } else if (!allowInexact) {
          this.raise(FlowErrors.InexactInsideExact, {
            at: this.state.lastTokStartLoc
          });
        }
        if (variance) {
          this.raise(FlowErrors.InexactVariance, {
            at: variance
          });
        }
        return null;
      }
      if (!allowSpread) {
        this.raise(FlowErrors.UnexpectedSpreadType, {
          at: this.state.lastTokStartLoc
        });
      }
      if (protoStartLoc != null) {
        this.unexpected(protoStartLoc);
      }
      if (variance) {
        this.raise(FlowErrors.SpreadVariance, {
          at: variance
        });
      }
      node.argument = this.flowParseType();
      return this.finishNode(node, "ObjectTypeSpreadProperty");
    } else {
      node.key = this.flowParseObjectPropertyKey();
      node.static = isStatic;
      node.proto = protoStartLoc != null;
      node.kind = kind;
      let optional = false;
      if (this.match(47) || this.match(10)) {
        node.method = true;
        if (protoStartLoc != null) {
          this.unexpected(protoStartLoc);
        }
        if (variance) {
          this.unexpected(variance.loc.start);
        }
        node.value = this.flowParseObjectTypeMethodish(this.startNodeAt(node.loc.start));
        if (kind === "get" || kind === "set") {
          this.flowCheckGetterSetterParams(node);
        }
        if (!allowSpread && node.key.name === "constructor" && node.value.this) {
          this.raise(FlowErrors.ThisParamBannedInConstructor, {
            at: node.value.this
          });
        }
      } else {
        if (kind !== "init") this.unexpected();
        node.method = false;
        if (this.eat(17)) {
          optional = true;
        }
        node.value = this.flowParseTypeInitialiser();
        node.variance = variance;
      }
      node.optional = optional;
      return this.finishNode(node, "ObjectTypeProperty");
    }
  }
  flowCheckGetterSetterParams(property) {
    const paramCount = property.kind === "get" ? 0 : 1;
    const length = property.value.params.length + (property.value.rest ? 1 : 0);
    if (property.value.this) {
      this.raise(property.kind === "get" ? FlowErrors.GetterMayNotHaveThisParam : FlowErrors.SetterMayNotHaveThisParam, {
        at: property.value.this
      });
    }
    if (length !== paramCount) {
      this.raise(property.kind === "get" ? _parseError.Errors.BadGetterArity : _parseError.Errors.BadSetterArity, {
        at: property
      });
    }
    if (property.kind === "set" && property.value.rest) {
      this.raise(_parseError.Errors.BadSetterRestParameter, {
        at: property
      });
    }
  }
  flowObjectTypeSemicolon() {
    if (!this.eat(13) && !this.eat(12) && !this.match(8) && !this.match(9)) {
      this.unexpected();
    }
  }
  flowParseQualifiedTypeIdentifier(startLoc, id) {
    var _startLoc;
    (_startLoc = startLoc) != null ? _startLoc : startLoc = this.state.startLoc;
    let node = id || this.flowParseRestrictedIdentifier(true);
    while (this.eat(16)) {
      const node2 = this.startNodeAt(startLoc);
      node2.qualification = node;
      node2.id = this.flowParseRestrictedIdentifier(true);
      node = this.finishNode(node2, "QualifiedTypeIdentifier");
    }
    return node;
  }
  flowParseGenericType(startLoc, id) {
    const node = this.startNodeAt(startLoc);
    node.typeParameters = null;
    node.id = this.flowParseQualifiedTypeIdentifier(startLoc, id);
    if (this.match(47)) {
      node.typeParameters = this.flowParseTypeParameterInstantiation();
    }
    return this.finishNode(node, "GenericTypeAnnotation");
  }
  flowParseTypeofType() {
    const node = this.startNode();
    this.expect(87);
    node.argument = this.flowParsePrimaryType();
    return this.finishNode(node, "TypeofTypeAnnotation");
  }
  flowParseTupleType() {
    const node = this.startNode();
    node.types = [];
    this.expect(0);
    while (this.state.pos < this.length && !this.match(3)) {
      node.types.push(this.flowParseType());
      if (this.match(3)) break;
      this.expect(12);
    }
    this.expect(3);
    return this.finishNode(node, "TupleTypeAnnotation");
  }
  flowParseFunctionTypeParam(first) {
    let name = null;
    let optional = false;
    let typeAnnotation = null;
    const node = this.startNode();
    const lh = this.lookahead();
    const isThis = this.state.type === 78;
    if (lh.type === 14 || lh.type === 17) {
      if (isThis && !first) {
        this.raise(FlowErrors.ThisParamMustBeFirst, {
          at: node
        });
      }
      name = this.parseIdentifier(isThis);
      if (this.eat(17)) {
        optional = true;
        if (isThis) {
          this.raise(FlowErrors.ThisParamMayNotBeOptional, {
            at: node
          });
        }
      }
      typeAnnotation = this.flowParseTypeInitialiser();
    } else {
      typeAnnotation = this.flowParseType();
    }
    node.name = name;
    node.optional = optional;
    node.typeAnnotation = typeAnnotation;
    return this.finishNode(node, "FunctionTypeParam");
  }
  reinterpretTypeAsFunctionTypeParam(type) {
    const node = this.startNodeAt(type.loc.start);
    node.name = null;
    node.optional = false;
    node.typeAnnotation = type;
    return this.finishNode(node, "FunctionTypeParam");
  }
  flowParseFunctionTypeParams(params = []) {
    let rest = null;
    let _this = null;
    if (this.match(78)) {
      _this = this.flowParseFunctionTypeParam(true);
      _this.name = null;
      if (!this.match(11)) {
        this.expect(12);
      }
    }
    while (!this.match(11) && !this.match(21)) {
      params.push(this.flowParseFunctionTypeParam(false));
      if (!this.match(11)) {
        this.expect(12);
      }
    }
    if (this.eat(21)) {
      rest = this.flowParseFunctionTypeParam(false);
    }
    return {
      params,
      rest,
      _this
    };
  }
  flowIdentToTypeAnnotation(startLoc, node, id) {
    switch (id.name) {
      case "any":
        return this.finishNode(node, "AnyTypeAnnotation");
      case "bool":
      case "boolean":
        return this.finishNode(node, "BooleanTypeAnnotation");
      case "mixed":
        return this.finishNode(node, "MixedTypeAnnotation");
      case "empty":
        return this.finishNode(node, "EmptyTypeAnnotation");
      case "number":
        return this.finishNode(node, "NumberTypeAnnotation");
      case "string":
        return this.finishNode(node, "StringTypeAnnotation");
      case "symbol":
        return this.finishNode(node, "SymbolTypeAnnotation");
      default:
        this.checkNotUnderscore(id.name);
        return this.flowParseGenericType(startLoc, id);
    }
  }
  flowParsePrimaryType() {
    const startLoc = this.state.startLoc;
    const node = this.startNode();
    let tmp;
    let type;
    let isGroupedType = false;
    const oldNoAnonFunctionType = this.state.noAnonFunctionType;
    switch (this.state.type) {
      case 5:
        return this.flowParseObjectType({
          allowStatic: false,
          allowExact: false,
          allowSpread: true,
          allowProto: false,
          allowInexact: true
        });
      case 6:
        return this.flowParseObjectType({
          allowStatic: false,
          allowExact: true,
          allowSpread: true,
          allowProto: false,
          allowInexact: false
        });
      case 0:
        this.state.noAnonFunctionType = false;
        type = this.flowParseTupleType();
        this.state.noAnonFunctionType = oldNoAnonFunctionType;
        return type;
      case 47:
        node.typeParameters = this.flowParseTypeParameterDeclaration();
        this.expect(10);
        tmp = this.flowParseFunctionTypeParams();
        node.params = tmp.params;
        node.rest = tmp.rest;
        node.this = tmp._this;
        this.expect(11);
        this.expect(19);
        node.returnType = this.flowParseType();
        return this.finishNode(node, "FunctionTypeAnnotation");
      case 10:
        this.next();
        if (!this.match(11) && !this.match(21)) {
          if ((0, _types.tokenIsIdentifier)(this.state.type) || this.match(78)) {
            const token = this.lookahead().type;
            isGroupedType = token !== 17 && token !== 14;
          } else {
            isGroupedType = true;
          }
        }
        if (isGroupedType) {
          this.state.noAnonFunctionType = false;
          type = this.flowParseType();
          this.state.noAnonFunctionType = oldNoAnonFunctionType;
          if (this.state.noAnonFunctionType || !(this.match(12) || this.match(11) && this.lookahead().type === 19)) {
            this.expect(11);
            return type;
          } else {
            this.eat(12);
          }
        }
        if (type) {
          tmp = this.flowParseFunctionTypeParams([this.reinterpretTypeAsFunctionTypeParam(type)]);
        } else {
          tmp = this.flowParseFunctionTypeParams();
        }
        node.params = tmp.params;
        node.rest = tmp.rest;
        node.this = tmp._this;
        this.expect(11);
        this.expect(19);
        node.returnType = this.flowParseType();
        node.typeParameters = null;
        return this.finishNode(node, "FunctionTypeAnnotation");
      case 131:
        return this.parseLiteral(this.state.value, "StringLiteralTypeAnnotation");
      case 85:
      case 86:
        node.value = this.match(85);
        this.next();
        return this.finishNode(node, "BooleanLiteralTypeAnnotation");
      case 53:
        if (this.state.value === "-") {
          this.next();
          if (this.match(132)) {
            return this.parseLiteralAtNode(-this.state.value, "NumberLiteralTypeAnnotation", node);
          }
          if (this.match(133)) {
            return this.parseLiteralAtNode(-this.state.value, "BigIntLiteralTypeAnnotation", node);
          }
          throw this.raise(FlowErrors.UnexpectedSubtractionOperand, {
            at: this.state.startLoc
          });
        }
        this.unexpected();
        return;
      case 132:
        return this.parseLiteral(this.state.value, "NumberLiteralTypeAnnotation");
      case 133:
        return this.parseLiteral(this.state.value, "BigIntLiteralTypeAnnotation");
      case 88:
        this.next();
        return this.finishNode(node, "VoidTypeAnnotation");
      case 84:
        this.next();
        return this.finishNode(node, "NullLiteralTypeAnnotation");
      case 78:
        this.next();
        return this.finishNode(node, "ThisTypeAnnotation");
      case 55:
        this.next();
        return this.finishNode(node, "ExistsTypeAnnotation");
      case 87:
        return this.flowParseTypeofType();
      default:
        if ((0, _types.tokenIsKeyword)(this.state.type)) {
          const label = (0, _types.tokenLabelName)(this.state.type);
          this.next();
          return super.createIdentifier(node, label);
        } else if ((0, _types.tokenIsIdentifier)(this.state.type)) {
          if (this.isContextual(127)) {
            return this.flowParseInterfaceType();
          }
          return this.flowIdentToTypeAnnotation(startLoc, node, this.parseIdentifier());
        }
    }
    this.unexpected();
  }
  flowParsePostfixType() {
    const startLoc = this.state.startLoc;
    let type = this.flowParsePrimaryType();
    let seenOptionalIndexedAccess = false;
    while ((this.match(0) || this.match(18)) && !this.canInsertSemicolon()) {
      const node = this.startNodeAt(startLoc);
      const optional = this.eat(18);
      seenOptionalIndexedAccess = seenOptionalIndexedAccess || optional;
      this.expect(0);
      if (!optional && this.match(3)) {
        node.elementType = type;
        this.next();
        type = this.finishNode(node, "ArrayTypeAnnotation");
      } else {
        node.objectType = type;
        node.indexType = this.flowParseType();
        this.expect(3);
        if (seenOptionalIndexedAccess) {
          node.optional = optional;
          type = this.finishNode(node, "OptionalIndexedAccessType");
        } else {
          type = this.finishNode(node, "IndexedAccessType");
        }
      }
    }
    return type;
  }
  flowParsePrefixType() {
    const node = this.startNode();
    if (this.eat(17)) {
      node.typeAnnotation = this.flowParsePrefixType();
      return this.finishNode(node, "NullableTypeAnnotation");
    } else {
      return this.flowParsePostfixType();
    }
  }
  flowParseAnonFunctionWithoutParens() {
    const param = this.flowParsePrefixType();
    if (!this.state.noAnonFunctionType && this.eat(19)) {
      const node = this.startNodeAt(param.loc.start);
      node.params = [this.reinterpretTypeAsFunctionTypeParam(param)];
      node.rest = null;
      node.this = null;
      node.returnType = this.flowParseType();
      node.typeParameters = null;
      return this.finishNode(node, "FunctionTypeAnnotation");
    }
    return param;
  }
  flowParseIntersectionType() {
    const node = this.startNode();
    this.eat(45);
    const type = this.flowParseAnonFunctionWithoutParens();
    node.types = [type];
    while (this.eat(45)) {
      node.types.push(this.flowParseAnonFunctionWithoutParens());
    }
    return node.types.length === 1 ? type : this.finishNode(node, "IntersectionTypeAnnotation");
  }
  flowParseUnionType() {
    const node = this.startNode();
    this.eat(43);
    const type = this.flowParseIntersectionType();
    node.types = [type];
    while (this.eat(43)) {
      node.types.push(this.flowParseIntersectionType());
    }
    return node.types.length === 1 ? type : this.finishNode(node, "UnionTypeAnnotation");
  }
  flowParseType() {
    const oldInType = this.state.inType;
    this.state.inType = true;
    const type = this.flowParseUnionType();
    this.state.inType = oldInType;
    return type;
  }
  flowParseTypeOrImplicitInstantiation() {
    if (this.state.type === 130 && this.state.value === "_") {
      const startLoc = this.state.startLoc;
      const node = this.parseIdentifier();
      return this.flowParseGenericType(startLoc, node);
    } else {
      return this.flowParseType();
    }
  }
  flowParseTypeAnnotation() {
    const node = this.startNode();
    node.typeAnnotation = this.flowParseTypeInitialiser();
    return this.finishNode(node, "TypeAnnotation");
  }
  flowParseTypeAnnotatableIdentifier(allowPrimitiveOverride) {
    const ident = allowPrimitiveOverride ? this.parseIdentifier() : this.flowParseRestrictedIdentifier();
    if (this.match(14)) {
      ident.typeAnnotation = this.flowParseTypeAnnotation();
      this.resetEndLocation(ident);
    }
    return ident;
  }
  typeCastToParameter(node) {
    node.expression.typeAnnotation = node.typeAnnotation;
    this.resetEndLocation(node.expression, node.typeAnnotation.loc.end);
    return node.expression;
  }
  flowParseVariance() {
    let variance = null;
    if (this.match(53)) {
      variance = this.startNode();
      if (this.state.value === "+") {
        variance.kind = "plus";
      } else {
        variance.kind = "minus";
      }
      this.next();
      return this.finishNode(variance, "Variance");
    }
    return variance;
  }
  parseFunctionBody(node, allowExpressionBody, isMethod = false) {
    if (allowExpressionBody) {
      this.forwardNoArrowParamsConversionAt(node, () => super.parseFunctionBody(node, true, isMethod));
      return;
    }
    super.parseFunctionBody(node, false, isMethod);
  }
  parseFunctionBodyAndFinish(node, type, isMethod = false) {
    if (this.match(14)) {
      const typeNode = this.startNode();
      [typeNode.typeAnnotation, node.predicate] = this.flowParseTypeAndPredicateInitialiser();
      node.returnType = typeNode.typeAnnotation ? this.finishNode(typeNode, "TypeAnnotation") : null;
    }
    return super.parseFunctionBodyAndFinish(node, type, isMethod);
  }
  parseStatementLike(flags) {
    if (this.state.strict && this.isContextual(127)) {
      const lookahead = this.lookahead();
      if ((0, _types.tokenIsKeywordOrIdentifier)(lookahead.type)) {
        const node = this.startNode();
        this.next();
        return this.flowParseInterface(node);
      }
    } else if (this.shouldParseEnums() && this.isContextual(124)) {
      const node = this.startNode();
      this.next();
      return this.flowParseEnumDeclaration(node);
    }
    const stmt = super.parseStatementLike(flags);
    if (this.flowPragma === undefined && !this.isValidDirective(stmt)) {
      this.flowPragma = null;
    }
    return stmt;
  }
  parseExpressionStatement(node, expr, decorators) {
    if (expr.type === "Identifier") {
      if (expr.name === "declare") {
        if (this.match(80) || (0, _types.tokenIsIdentifier)(this.state.type) || this.match(68) || this.match(74) || this.match(82)) {
          return this.flowParseDeclare(node);
        }
      } else if ((0, _types.tokenIsIdentifier)(this.state.type)) {
        if (expr.name === "interface") {
          return this.flowParseInterface(node);
        } else if (expr.name === "type") {
          return this.flowParseTypeAlias(node);
        } else if (expr.name === "opaque") {
          return this.flowParseOpaqueType(node, false);
        }
      }
    }
    return super.parseExpressionStatement(node, expr, decorators);
  }
  shouldParseExportDeclaration() {
    const {
      type
    } = this.state;
    if ((0, _types.tokenIsFlowInterfaceOrTypeOrOpaque)(type) || this.shouldParseEnums() && type === 124) {
      return !this.state.containsEsc;
    }
    return super.shouldParseExportDeclaration();
  }
  isExportDefaultSpecifier() {
    const {
      type
    } = this.state;
    if ((0, _types.tokenIsFlowInterfaceOrTypeOrOpaque)(type) || this.shouldParseEnums() && type === 124) {
      return this.state.containsEsc;
    }
    return super.isExportDefaultSpecifier();
  }
  parseExportDefaultExpression() {
    if (this.shouldParseEnums() && this.isContextual(124)) {
      const node = this.startNode();
      this.next();
      return this.flowParseEnumDeclaration(node);
    }
    return super.parseExportDefaultExpression();
  }
  parseConditional(expr, startLoc, refExpressionErrors) {
    if (!this.match(17)) return expr;
    if (this.state.maybeInArrowParameters) {
      const nextCh = this.lookaheadCharCode();
      if (nextCh === 44 || nextCh === 61 || nextCh === 58 || nextCh === 41) {
        this.setOptionalParametersError(refExpressionErrors);
        return expr;
      }
    }
    this.expect(17);
    const state = this.state.clone();
    const originalNoArrowAt = this.state.noArrowAt;
    const node = this.startNodeAt(startLoc);
    let {
      consequent,
      failed
    } = this.tryParseConditionalConsequent();
    let [valid, invalid] = this.getArrowLikeExpressions(consequent);
    if (failed || invalid.length > 0) {
      const noArrowAt = [...originalNoArrowAt];
      if (invalid.length > 0) {
        this.state = state;
        this.state.noArrowAt = noArrowAt;
        for (let i = 0; i < invalid.length; i++) {
          noArrowAt.push(invalid[i].start);
        }
        ({
          consequent,
          failed
        } = this.tryParseConditionalConsequent());
        [valid, invalid] = this.getArrowLikeExpressions(consequent);
      }
      if (failed && valid.length > 1) {
        this.raise(FlowErrors.AmbiguousConditionalArrow, {
          at: state.startLoc
        });
      }
      if (failed && valid.length === 1) {
        this.state = state;
        noArrowAt.push(valid[0].start);
        this.state.noArrowAt = noArrowAt;
        ({
          consequent,
          failed
        } = this.tryParseConditionalConsequent());
      }
    }
    this.getArrowLikeExpressions(consequent, true);
    this.state.noArrowAt = originalNoArrowAt;
    this.expect(14);
    node.test = expr;
    node.consequent = consequent;
    node.alternate = this.forwardNoArrowParamsConversionAt(node, () => this.parseMaybeAssign(undefined, undefined));
    return this.finishNode(node, "ConditionalExpression");
  }
  tryParseConditionalConsequent() {
    this.state.noArrowParamsConversionAt.push(this.state.start);
    const consequent = this.parseMaybeAssignAllowIn();
    const failed = !this.match(14);
    this.state.noArrowParamsConversionAt.pop();
    return {
      consequent,
      failed
    };
  }
  getArrowLikeExpressions(node, disallowInvalid) {
    const stack = [node];
    const arrows = [];
    while (stack.length !== 0) {
      const node = stack.pop();
      if (node.type === "ArrowFunctionExpression") {
        if (node.typeParameters || !node.returnType) {
          this.finishArrowValidation(node);
        } else {
          arrows.push(node);
        }
        stack.push(node.body);
      } else if (node.type === "ConditionalExpression") {
        stack.push(node.consequent);
        stack.push(node.alternate);
      }
    }
    if (disallowInvalid) {
      arrows.forEach(node => this.finishArrowValidation(node));
      return [arrows, []];
    }
    return partition(arrows, node => node.params.every(param => this.isAssignable(param, true)));
  }
  finishArrowValidation(node) {
    var _node$extra;
    this.toAssignableList(node.params, (_node$extra = node.extra) == null ? void 0 : _node$extra.trailingCommaLoc, false);
    this.scope.enter(_scopeflags.ScopeFlag.FUNCTION | _scopeflags.ScopeFlag.ARROW);
    super.checkParams(node, false, true);
    this.scope.exit();
  }
  forwardNoArrowParamsConversionAt(node, parse) {
    let result;
    if (this.state.noArrowParamsConversionAt.indexOf(node.start) !== -1) {
      this.state.noArrowParamsConversionAt.push(this.state.start);
      result = parse();
      this.state.noArrowParamsConversionAt.pop();
    } else {
      result = parse();
    }
    return result;
  }
  parseParenItem(node, startLoc) {
    node = super.parseParenItem(node, startLoc);
    if (this.eat(17)) {
      node.optional = true;
      this.resetEndLocation(node);
    }
    if (this.match(14)) {
      const typeCastNode = this.startNodeAt(startLoc);
      typeCastNode.expression = node;
      typeCastNode.typeAnnotation = this.flowParseTypeAnnotation();
      return this.finishNode(typeCastNode, "TypeCastExpression");
    }
    return node;
  }
  assertModuleNodeAllowed(node) {
    if (node.type === "ImportDeclaration" && (node.importKind === "type" || node.importKind === "typeof") || node.type === "ExportNamedDeclaration" && node.exportKind === "type" || node.type === "ExportAllDeclaration" && node.exportKind === "type") {
      return;
    }
    super.assertModuleNodeAllowed(node);
  }
  parseExportDeclaration(node) {
    if (this.isContextual(128)) {
      node.exportKind = "type";
      const declarationNode = this.startNode();
      this.next();
      if (this.match(5)) {
        node.specifiers = this.parseExportSpecifiers(true);
        super.parseExportFrom(node);
        return null;
      } else {
        return this.flowParseTypeAlias(declarationNode);
      }
    } else if (this.isContextual(129)) {
      node.exportKind = "type";
      const declarationNode = this.startNode();
      this.next();
      return this.flowParseOpaqueType(declarationNode, false);
    } else if (this.isContextual(127)) {
      node.exportKind = "type";
      const declarationNode = this.startNode();
      this.next();
      return this.flowParseInterface(declarationNode);
    } else if (this.shouldParseEnums() && this.isContextual(124)) {
      node.exportKind = "value";
      const declarationNode = this.startNode();
      this.next();
      return this.flowParseEnumDeclaration(declarationNode);
    } else {
      return super.parseExportDeclaration(node);
    }
  }
  eatExportStar(node) {
    if (super.eatExportStar(node)) return true;
    if (this.isContextual(128) && this.lookahead().type === 55) {
      node.exportKind = "type";
      this.next();
      this.next();
      return true;
    }
    return false;
  }
  maybeParseExportNamespaceSpecifier(node) {
    const {
      startLoc
    } = this.state;
    const hasNamespace = super.maybeParseExportNamespaceSpecifier(node);
    if (hasNamespace && node.exportKind === "type") {
      this.unexpected(startLoc);
    }
    return hasNamespace;
  }
  parseClassId(node, isStatement, optionalId) {
    super.parseClassId(node, isStatement, optionalId);
    if (this.match(47)) {
      node.typeParameters = this.flowParseTypeParameterDeclaration();
    }
  }
  parseClassMember(classBody, member, state) {
    const {
      startLoc
    } = this.state;
    if (this.isContextual(123)) {
      if (super.parseClassMemberFromModifier(classBody, member)) {
        return;
      }
      member.declare = true;
    }
    super.parseClassMember(classBody, member, state);
    if (member.declare) {
      if (member.type !== "ClassProperty" && member.type !== "ClassPrivateProperty" && member.type !== "PropertyDefinition") {
        this.raise(FlowErrors.DeclareClassElement, {
          at: startLoc
        });
      } else if (member.value) {
        this.raise(FlowErrors.DeclareClassFieldInitializer, {
          at: member.value
        });
      }
    }
  }
  isIterator(word) {
    return word === "iterator" || word === "asyncIterator";
  }
  readIterator() {
    const word = super.readWord1();
    const fullWord = "@@" + word;
    if (!this.isIterator(word) || !this.state.inType) {
      this.raise(_parseError.Errors.InvalidIdentifier, {
        at: this.state.curPosition(),
        identifierName: fullWord
      });
    }
    this.finishToken(130, fullWord);
  }
  getTokenFromCode(code) {
    const next = this.input.charCodeAt(this.state.pos + 1);
    if (code === 123 && next === 124) {
      this.finishOp(6, 2);
    } else if (this.state.inType && (code === 62 || code === 60)) {
      this.finishOp(code === 62 ? 48 : 47, 1);
    } else if (this.state.inType && code === 63) {
      if (next === 46) {
        this.finishOp(18, 2);
      } else {
        this.finishOp(17, 1);
      }
    } else if ((0, _identifier.isIteratorStart)(code, next, this.input.charCodeAt(this.state.pos + 2))) {
      this.state.pos += 2;
      this.readIterator();
    } else {
      super.getTokenFromCode(code);
    }
  }
  isAssignable(node, isBinding) {
    if (node.type === "TypeCastExpression") {
      return this.isAssignable(node.expression, isBinding);
    } else {
      return super.isAssignable(node, isBinding);
    }
  }
  toAssignable(node, isLHS = false) {
    if (!isLHS && node.type === "AssignmentExpression" && node.left.type === "TypeCastExpression") {
      node.left = this.typeCastToParameter(node.left);
    }
    super.toAssignable(node, isLHS);
  }
  toAssignableList(exprList, trailingCommaLoc, isLHS) {
    for (let i = 0; i < exprList.length; i++) {
      const expr = exprList[i];
      if ((expr == null ? void 0 : expr.type) === "TypeCastExpression") {
        exprList[i] = this.typeCastToParameter(expr);
      }
    }
    super.toAssignableList(exprList, trailingCommaLoc, isLHS);
  }
  toReferencedList(exprList, isParenthesizedExpr) {
    for (let i = 0; i < exprList.length; i++) {
      var _expr$extra;
      const expr = exprList[i];
      if (expr && expr.type === "TypeCastExpression" && !((_expr$extra = expr.extra) != null && _expr$extra.parenthesized) && (exprList.length > 1 || !isParenthesizedExpr)) {
        this.raise(FlowErrors.TypeCastInPattern, {
          at: expr.typeAnnotation
        });
      }
    }
    return exprList;
  }
  parseArrayLike(close, canBePattern, isTuple, refExpressionErrors) {
    const node = super.parseArrayLike(close, canBePattern, isTuple, refExpressionErrors);
    if (canBePattern && !this.state.maybeInArrowParameters) {
      this.toReferencedList(node.elements);
    }
    return node;
  }
  isValidLVal(type, isParenthesized, binding) {
    return type === "TypeCastExpression" || super.isValidLVal(type, isParenthesized, binding);
  }
  parseClassProperty(node) {
    if (this.match(14)) {
      node.typeAnnotation = this.flowParseTypeAnnotation();
    }
    return super.parseClassProperty(node);
  }
  parseClassPrivateProperty(node) {
    if (this.match(14)) {
      node.typeAnnotation = this.flowParseTypeAnnotation();
    }
    return super.parseClassPrivateProperty(node);
  }
  isClassMethod() {
    return this.match(47) || super.isClassMethod();
  }
  isClassProperty() {
    return this.match(14) || super.isClassProperty();
  }
  isNonstaticConstructor(method) {
    return !this.match(14) && super.isNonstaticConstructor(method);
  }
  pushClassMethod(classBody, method, isGenerator, isAsync, isConstructor, allowsDirectSuper) {
    if (method.variance) {
      this.unexpected(method.variance.loc.start);
    }
    delete method.variance;
    if (this.match(47)) {
      method.typeParameters = this.flowParseTypeParameterDeclaration();
    }
    super.pushClassMethod(classBody, method, isGenerator, isAsync, isConstructor, allowsDirectSuper);
    if (method.params && isConstructor) {
      const params = method.params;
      if (params.length > 0 && this.isThisParam(params[0])) {
        this.raise(FlowErrors.ThisParamBannedInConstructor, {
          at: method
        });
      }
    } else if (method.type === "MethodDefinition" && isConstructor && method.value.params) {
      const params = method.value.params;
      if (params.length > 0 && this.isThisParam(params[0])) {
        this.raise(FlowErrors.ThisParamBannedInConstructor, {
          at: method
        });
      }
    }
  }
  pushClassPrivateMethod(classBody, method, isGenerator, isAsync) {
    if (method.variance) {
      this.unexpected(method.variance.loc.start);
    }
    delete method.variance;
    if (this.match(47)) {
      method.typeParameters = this.flowParseTypeParameterDeclaration();
    }
    super.pushClassPrivateMethod(classBody, method, isGenerator, isAsync);
  }
  parseClassSuper(node) {
    super.parseClassSuper(node);
    if (node.superClass && this.match(47)) {
      node.superTypeParameters = this.flowParseTypeParameterInstantiation();
    }
    if (this.isContextual(111)) {
      this.next();
      const implemented = node.implements = [];
      do {
        const node = this.startNode();
        node.id = this.flowParseRestrictedIdentifier(true);
        if (this.match(47)) {
          node.typeParameters = this.flowParseTypeParameterInstantiation();
        } else {
          node.typeParameters = null;
        }
        implemented.push(this.finishNode(node, "ClassImplements"));
      } while (this.eat(12));
    }
  }
  checkGetterSetterParams(method) {
    super.checkGetterSetterParams(method);
    const params = this.getObjectOrClassMethodParams(method);
    if (params.length > 0) {
      const param = params[0];
      if (this.isThisParam(param) && method.kind === "get") {
        this.raise(FlowErrors.GetterMayNotHaveThisParam, {
          at: param
        });
      } else if (this.isThisParam(param)) {
        this.raise(FlowErrors.SetterMayNotHaveThisParam, {
          at: param
        });
      }
    }
  }
  parsePropertyNamePrefixOperator(node) {
    node.variance = this.flowParseVariance();
  }
  parseObjPropValue(prop, startLoc, isGenerator, isAsync, isPattern, isAccessor, refExpressionErrors) {
    if (prop.variance) {
      this.unexpected(prop.variance.loc.start);
    }
    delete prop.variance;
    let typeParameters;
    if (this.match(47) && !isAccessor) {
      typeParameters = this.flowParseTypeParameterDeclaration();
      if (!this.match(10)) this.unexpected();
    }
    const result = super.parseObjPropValue(prop, startLoc, isGenerator, isAsync, isPattern, isAccessor, refExpressionErrors);
    if (typeParameters) {
      (result.value || result).typeParameters = typeParameters;
    }
    return result;
  }
  parseAssignableListItemTypes(param) {
    if (this.eat(17)) {
      if (param.type !== "Identifier") {
        this.raise(FlowErrors.PatternIsOptional, {
          at: param
        });
      }
      if (this.isThisParam(param)) {
        this.raise(FlowErrors.ThisParamMayNotBeOptional, {
          at: param
        });
      }
      param.optional = true;
    }
    if (this.match(14)) {
      param.typeAnnotation = this.flowParseTypeAnnotation();
    } else if (this.isThisParam(param)) {
      this.raise(FlowErrors.ThisParamAnnotationRequired, {
        at: param
      });
    }
    if (this.match(29) && this.isThisParam(param)) {
      this.raise(FlowErrors.ThisParamNoDefault, {
        at: param
      });
    }
    this.resetEndLocation(param);
    return param;
  }
  parseMaybeDefault(startLoc, left) {
    const node = super.parseMaybeDefault(startLoc, left);
    if (node.type === "AssignmentPattern" && node.typeAnnotation && node.right.start < node.typeAnnotation.start) {
      this.raise(FlowErrors.TypeBeforeInitializer, {
        at: node.typeAnnotation
      });
    }
    return node;
  }
  checkImportReflection(node) {
    super.checkImportReflection(node);
    if (node.module && node.importKind !== "value") {
      this.raise(FlowErrors.ImportReflectionHasImportType, {
        at: node.specifiers[0].loc.start
      });
    }
  }
  parseImportSpecifierLocal(node, specifier, type) {
    specifier.local = hasTypeImportKind(node) ? this.flowParseRestrictedIdentifier(true, true) : this.parseIdentifier();
    node.specifiers.push(this.finishImportSpecifier(specifier, type));
  }
  isPotentialImportPhase(isExport) {
    if (super.isPotentialImportPhase(isExport)) return true;
    if (this.isContextual(128)) {
      if (!isExport) return true;
      const ch = this.lookaheadCharCode();
      return ch === 123 || ch === 42;
    }
    return !isExport && this.isContextual(87);
  }
  applyImportPhase(node, isExport, phase, loc) {
    super.applyImportPhase(node, isExport, phase, loc);
    if (isExport) {
      if (!phase && this.match(65)) {
        return;
      }
      node.exportKind = phase === "type" ? phase : "value";
    } else {
      if (phase === "type" && this.match(55)) this.unexpected();
      node.importKind = phase === "type" || phase === "typeof" ? phase : "value";
    }
  }
  parseImportSpecifier(specifier, importedIsString, isInTypeOnlyImport, isMaybeTypeOnly, bindingType) {
    const firstIdent = specifier.imported;
    let specifierTypeKind = null;
    if (firstIdent.type === "Identifier") {
      if (firstIdent.name === "type") {
        specifierTypeKind = "type";
      } else if (firstIdent.name === "typeof") {
        specifierTypeKind = "typeof";
      }
    }
    let isBinding = false;
    if (this.isContextual(93) && !this.isLookaheadContextual("as")) {
      const as_ident = this.parseIdentifier(true);
      if (specifierTypeKind !== null && !(0, _types.tokenIsKeywordOrIdentifier)(this.state.type)) {
        specifier.imported = as_ident;
        specifier.importKind = specifierTypeKind;
        specifier.local = (0, _node.cloneIdentifier)(as_ident);
      } else {
        specifier.imported = firstIdent;
        specifier.importKind = null;
        specifier.local = this.parseIdentifier();
      }
    } else {
      if (specifierTypeKind !== null && (0, _types.tokenIsKeywordOrIdentifier)(this.state.type)) {
        specifier.imported = this.parseIdentifier(true);
        specifier.importKind = specifierTypeKind;
      } else {
        if (importedIsString) {
          throw this.raise(_parseError.Errors.ImportBindingIsString, {
            at: specifier,
            importName: firstIdent.value
          });
        }
        specifier.imported = firstIdent;
        specifier.importKind = null;
      }
      if (this.eatContextual(93)) {
        specifier.local = this.parseIdentifier();
      } else {
        isBinding = true;
        specifier.local = (0, _node.cloneIdentifier)(specifier.imported);
      }
    }
    const specifierIsTypeImport = hasTypeImportKind(specifier);
    if (isInTypeOnlyImport && specifierIsTypeImport) {
      this.raise(FlowErrors.ImportTypeShorthandOnlyInPureImport, {
        at: specifier
      });
    }
    if (isInTypeOnlyImport || specifierIsTypeImport) {
      this.checkReservedType(specifier.local.name, specifier.local.loc.start, true);
    }
    if (isBinding && !isInTypeOnlyImport && !specifierIsTypeImport) {
      this.checkReservedWord(specifier.local.name, specifier.loc.start, true, true);
    }
    return this.finishImportSpecifier(specifier, "ImportSpecifier");
  }
  parseBindingAtom() {
    switch (this.state.type) {
      case 78:
        return this.parseIdentifier(true);
      default:
        return super.parseBindingAtom();
    }
  }
  parseFunctionParams(node, isConstructor) {
    const kind = node.kind;
    if (kind !== "get" && kind !== "set" && this.match(47)) {
      node.typeParameters = this.flowParseTypeParameterDeclaration();
    }
    super.parseFunctionParams(node, isConstructor);
  }
  parseVarId(decl, kind) {
    super.parseVarId(decl, kind);
    if (this.match(14)) {
      decl.id.typeAnnotation = this.flowParseTypeAnnotation();
      this.resetEndLocation(decl.id);
    }
  }
  parseAsyncArrowFromCallExpression(node, call) {
    if (this.match(14)) {
      const oldNoAnonFunctionType = this.state.noAnonFunctionType;
      this.state.noAnonFunctionType = true;
      node.returnType = this.flowParseTypeAnnotation();
      this.state.noAnonFunctionType = oldNoAnonFunctionType;
    }
    return super.parseAsyncArrowFromCallExpression(node, call);
  }
  shouldParseAsyncArrow() {
    return this.match(14) || super.shouldParseAsyncArrow();
  }
  parseMaybeAssign(refExpressionErrors, afterLeftParse) {
    var _jsx;
    let state = null;
    let jsx;
    if (this.hasPlugin("jsx") && (this.match(140) || this.match(47))) {
      state = this.state.clone();
      jsx = this.tryParse(() => super.parseMaybeAssign(refExpressionErrors, afterLeftParse), state);
      if (!jsx.error) return jsx.node;
      const {
        context
      } = this.state;
      const currentContext = context[context.length - 1];
      if (currentContext === _context.types.j_oTag || currentContext === _context.types.j_expr) {
        context.pop();
      }
    }
    if ((_jsx = jsx) != null && _jsx.error || this.match(47)) {
      var _jsx2, _jsx3;
      state = state || this.state.clone();
      let typeParameters;
      const arrow = this.tryParse(abort => {
        var _arrowExpression$extr;
        typeParameters = this.flowParseTypeParameterDeclaration();
        const arrowExpression = this.forwardNoArrowParamsConversionAt(typeParameters, () => {
          const result = super.parseMaybeAssign(refExpressionErrors, afterLeftParse);
          this.resetStartLocationFromNode(result, typeParameters);
          return result;
        });
        if ((_arrowExpression$extr = arrowExpression.extra) != null && _arrowExpression$extr.parenthesized) abort();
        const expr = this.maybeUnwrapTypeCastExpression(arrowExpression);
        if (expr.type !== "ArrowFunctionExpression") abort();
        expr.typeParameters = typeParameters;
        this.resetStartLocationFromNode(expr, typeParameters);
        return arrowExpression;
      }, state);
      let arrowExpression = null;
      if (arrow.node && this.maybeUnwrapTypeCastExpression(arrow.node).type === "ArrowFunctionExpression") {
        if (!arrow.error && !arrow.aborted) {
          if (arrow.node.async) {
            this.raise(FlowErrors.UnexpectedTypeParameterBeforeAsyncArrowFunction, {
              at: typeParameters
            });
          }
          return arrow.node;
        }
        arrowExpression = arrow.node;
      }
      if ((_jsx2 = jsx) != null && _jsx2.node) {
        this.state = jsx.failState;
        return jsx.node;
      }
      if (arrowExpression) {
        this.state = arrow.failState;
        return arrowExpression;
      }
      if ((_jsx3 = jsx) != null && _jsx3.thrown) throw jsx.error;
      if (arrow.thrown) throw arrow.error;
      throw this.raise(FlowErrors.UnexpectedTokenAfterTypeParameter, {
        at: typeParameters
      });
    }
    return super.parseMaybeAssign(refExpressionErrors, afterLeftParse);
  }
  parseArrow(node) {
    if (this.match(14)) {
      const result = this.tryParse(() => {
        const oldNoAnonFunctionType = this.state.noAnonFunctionType;
        this.state.noAnonFunctionType = true;
        const typeNode = this.startNode();
        [typeNode.typeAnnotation, node.predicate] = this.flowParseTypeAndPredicateInitialiser();
        this.state.noAnonFunctionType = oldNoAnonFunctionType;
        if (this.canInsertSemicolon()) this.unexpected();
        if (!this.match(19)) this.unexpected();
        return typeNode;
      });
      if (result.thrown) return null;
      if (result.error) this.state = result.failState;
      node.returnType = result.node.typeAnnotation ? this.finishNode(result.node, "TypeAnnotation") : null;
    }
    return super.parseArrow(node);
  }
  shouldParseArrow(params) {
    return this.match(14) || super.shouldParseArrow(params);
  }
  setArrowFunctionParameters(node, params) {
    if (this.state.noArrowParamsConversionAt.indexOf(node.start) !== -1) {
      node.params = params;
    } else {
      super.setArrowFunctionParameters(node, params);
    }
  }
  checkParams(node, allowDuplicates, isArrowFunction, strictModeChanged = true) {
    if (isArrowFunction && this.state.noArrowParamsConversionAt.indexOf(node.start) !== -1) {
      return;
    }
    for (let i = 0; i < node.params.length; i++) {
      if (this.isThisParam(node.params[i]) && i > 0) {
        this.raise(FlowErrors.ThisParamMustBeFirst, {
          at: node.params[i]
        });
      }
    }
    super.checkParams(node, allowDuplicates, isArrowFunction, strictModeChanged);
  }
  parseParenAndDistinguishExpression(canBeArrow) {
    return super.parseParenAndDistinguishExpression(canBeArrow && this.state.noArrowAt.indexOf(this.state.start) === -1);
  }
  parseSubscripts(base, startLoc, noCalls) {
    if (base.type === "Identifier" && base.name === "async" && this.state.noArrowAt.indexOf(startLoc.index) !== -1) {
      this.next();
      const node = this.startNodeAt(startLoc);
      node.callee = base;
      node.arguments = super.parseCallExpressionArguments(11, false);
      base = this.finishNode(node, "CallExpression");
    } else if (base.type === "Identifier" && base.name === "async" && this.match(47)) {
      const state = this.state.clone();
      const arrow = this.tryParse(abort => this.parseAsyncArrowWithTypeParameters(startLoc) || abort(), state);
      if (!arrow.error && !arrow.aborted) return arrow.node;
      const result = this.tryParse(() => super.parseSubscripts(base, startLoc, noCalls), state);
      if (result.node && !result.error) return result.node;
      if (arrow.node) {
        this.state = arrow.failState;
        return arrow.node;
      }
      if (result.node) {
        this.state = result.failState;
        return result.node;
      }
      throw arrow.error || result.error;
    }
    return super.parseSubscripts(base, startLoc, noCalls);
  }
  parseSubscript(base, startLoc, noCalls, subscriptState) {
    if (this.match(18) && this.isLookaheadToken_lt()) {
      subscriptState.optionalChainMember = true;
      if (noCalls) {
        subscriptState.stop = true;
        return base;
      }
      this.next();
      const node = this.startNodeAt(startLoc);
      node.callee = base;
      node.typeArguments = this.flowParseTypeParameterInstantiation();
      this.expect(10);
      node.arguments = this.parseCallExpressionArguments(11, false);
      node.optional = true;
      return this.finishCallExpression(node, true);
    } else if (!noCalls && this.shouldParseTypes() && this.match(47)) {
      const node = this.startNodeAt(startLoc);
      node.callee = base;
      const result = this.tryParse(() => {
        node.typeArguments = this.flowParseTypeParameterInstantiationCallOrNew();
        this.expect(10);
        node.arguments = super.parseCallExpressionArguments(11, false);
        if (subscriptState.optionalChainMember) {
          node.optional = false;
        }
        return this.finishCallExpression(node, subscriptState.optionalChainMember);
      });
      if (result.node) {
        if (result.error) this.state = result.failState;
        return result.node;
      }
    }
    return super.parseSubscript(base, startLoc, noCalls, subscriptState);
  }
  parseNewCallee(node) {
    super.parseNewCallee(node);
    let targs = null;
    if (this.shouldParseTypes() && this.match(47)) {
      targs = this.tryParse(() => this.flowParseTypeParameterInstantiationCallOrNew()).node;
    }
    node.typeArguments = targs;
  }
  parseAsyncArrowWithTypeParameters(startLoc) {
    const node = this.startNodeAt(startLoc);
    this.parseFunctionParams(node, false);
    if (!this.parseArrow(node)) return;
    return super.parseArrowExpression(node, undefined, true);
  }
  readToken_mult_modulo(code) {
    const next = this.input.charCodeAt(this.state.pos + 1);
    if (code === 42 && next === 47 && this.state.hasFlowComment) {
      this.state.hasFlowComment = false;
      this.state.pos += 2;
      this.nextToken();
      return;
    }
    super.readToken_mult_modulo(code);
  }
  readToken_pipe_amp(code) {
    const next = this.input.charCodeAt(this.state.pos + 1);
    if (code === 124 && next === 125) {
      this.finishOp(9, 2);
      return;
    }
    super.readToken_pipe_amp(code);
  }
  parseTopLevel(file, program) {
    const fileNode = super.parseTopLevel(file, program);
    if (this.state.hasFlowComment) {
      this.raise(FlowErrors.UnterminatedFlowComment, {
        at: this.state.curPosition()
      });
    }
    return fileNode;
  }
  skipBlockComment() {
    if (this.hasPlugin("flowComments") && this.skipFlowComment()) {
      if (this.state.hasFlowComment) {
        throw this.raise(FlowErrors.NestedFlowComment, {
          at: this.state.startLoc
        });
      }
      this.hasFlowCommentCompletion();
      const commentSkip = this.skipFlowComment();
      if (commentSkip) {
        this.state.pos += commentSkip;
        this.state.hasFlowComment = true;
      }
      return;
    }
    return super.skipBlockComment(this.state.hasFlowComment ? "*-/" : "*/");
  }
  skipFlowComment() {
    const {
      pos
    } = this.state;
    let shiftToFirstNonWhiteSpace = 2;
    while ([32, 9].includes(this.input.charCodeAt(pos + shiftToFirstNonWhiteSpace))) {
      shiftToFirstNonWhiteSpace++;
    }
    const ch2 = this.input.charCodeAt(shiftToFirstNonWhiteSpace + pos);
    const ch3 = this.input.charCodeAt(shiftToFirstNonWhiteSpace + pos + 1);
    if (ch2 === 58 && ch3 === 58) {
      return shiftToFirstNonWhiteSpace + 2;
    }
    if (this.input.slice(shiftToFirstNonWhiteSpace + pos, shiftToFirstNonWhiteSpace + pos + 12) === "flow-include") {
      return shiftToFirstNonWhiteSpace + 12;
    }
    if (ch2 === 58 && ch3 !== 58) {
      return shiftToFirstNonWhiteSpace;
    }
    return false;
  }
  hasFlowCommentCompletion() {
    const end = this.input.indexOf("*/", this.state.pos);
    if (end === -1) {
      throw this.raise(_parseError.Errors.UnterminatedComment, {
        at: this.state.curPosition()
      });
    }
  }
  flowEnumErrorBooleanMemberNotInitialized(loc, {
    enumName,
    memberName
  }) {
    this.raise(FlowErrors.EnumBooleanMemberNotInitialized, {
      at: loc,
      memberName,
      enumName
    });
  }
  flowEnumErrorInvalidMemberInitializer(loc, enumContext) {
    return this.raise(!enumContext.explicitType ? FlowErrors.EnumInvalidMemberInitializerUnknownType : enumContext.explicitType === "symbol" ? FlowErrors.EnumInvalidMemberInitializerSymbolType : FlowErrors.EnumInvalidMemberInitializerPrimaryType, Object.assign({
      at: loc
    }, enumContext));
  }
  flowEnumErrorNumberMemberNotInitialized(loc, {
    enumName,
    memberName
  }) {
    this.raise(FlowErrors.EnumNumberMemberNotInitialized, {
      at: loc,
      enumName,
      memberName
    });
  }
  flowEnumErrorStringMemberInconsistentlyInitailized(node, {
    enumName
  }) {
    this.raise(FlowErrors.EnumStringMemberInconsistentlyInitailized, {
      at: node,
      enumName
    });
  }
  flowEnumMemberInit() {
    const startLoc = this.state.startLoc;
    const endOfInit = () => this.match(12) || this.match(8);
    switch (this.state.type) {
      case 132:
        {
          const literal = this.parseNumericLiteral(this.state.value);
          if (endOfInit()) {
            return {
              type: "number",
              loc: literal.loc.start,
              value: literal
            };
          }
          return {
            type: "invalid",
            loc: startLoc
          };
        }
      case 131:
        {
          const literal = this.parseStringLiteral(this.state.value);
          if (endOfInit()) {
            return {
              type: "string",
              loc: literal.loc.start,
              value: literal
            };
          }
          return {
            type: "invalid",
            loc: startLoc
          };
        }
      case 85:
      case 86:
        {
          const literal = this.parseBooleanLiteral(this.match(85));
          if (endOfInit()) {
            return {
              type: "boolean",
              loc: literal.loc.start,
              value: literal
            };
          }
          return {
            type: "invalid",
            loc: startLoc
          };
        }
      default:
        return {
          type: "invalid",
          loc: startLoc
        };
    }
  }
  flowEnumMemberRaw() {
    const loc = this.state.startLoc;
    const id = this.parseIdentifier(true);
    const init = this.eat(29) ? this.flowEnumMemberInit() : {
      type: "none",
      loc
    };
    return {
      id,
      init
    };
  }
  flowEnumCheckExplicitTypeMismatch(loc, context, expectedType) {
    const {
      explicitType
    } = context;
    if (explicitType === null) {
      return;
    }
    if (explicitType !== expectedType) {
      this.flowEnumErrorInvalidMemberInitializer(loc, context);
    }
  }
  flowEnumMembers({
    enumName,
    explicitType
  }) {
    const seenNames = new Set();
    const members = {
      booleanMembers: [],
      numberMembers: [],
      stringMembers: [],
      defaultedMembers: []
    };
    let hasUnknownMembers = false;
    while (!this.match(8)) {
      if (this.eat(21)) {
        hasUnknownMembers = true;
        break;
      }
      const memberNode = this.startNode();
      const {
        id,
        init
      } = this.flowEnumMemberRaw();
      const memberName = id.name;
      if (memberName === "") {
        continue;
      }
      if (/^[a-z]/.test(memberName)) {
        this.raise(FlowErrors.EnumInvalidMemberName, {
          at: id,
          memberName,
          suggestion: memberName[0].toUpperCase() + memberName.slice(1),
          enumName
        });
      }
      if (seenNames.has(memberName)) {
        this.raise(FlowErrors.EnumDuplicateMemberName, {
          at: id,
          memberName,
          enumName
        });
      }
      seenNames.add(memberName);
      const context = {
        enumName,
        explicitType,
        memberName
      };
      memberNode.id = id;
      switch (init.type) {
        case "boolean":
          {
            this.flowEnumCheckExplicitTypeMismatch(init.loc, context, "boolean");
            memberNode.init = init.value;
            members.booleanMembers.push(this.finishNode(memberNode, "EnumBooleanMember"));
            break;
          }
        case "number":
          {
            this.flowEnumCheckExplicitTypeMismatch(init.loc, context, "number");
            memberNode.init = init.value;
            members.numberMembers.push(this.finishNode(memberNode, "EnumNumberMember"));
            break;
          }
        case "string":
          {
            this.flowEnumCheckExplicitTypeMismatch(init.loc, context, "string");
            memberNode.init = init.value;
            members.stringMembers.push(this.finishNode(memberNode, "EnumStringMember"));
            break;
          }
        case "invalid":
          {
            throw this.flowEnumErrorInvalidMemberInitializer(init.loc, context);
          }
        case "none":
          {
            switch (explicitType) {
              case "boolean":
                this.flowEnumErrorBooleanMemberNotInitialized(init.loc, context);
                break;
              case "number":
                this.flowEnumErrorNumberMemberNotInitialized(init.loc, context);
                break;
              default:
                members.defaultedMembers.push(this.finishNode(memberNode, "EnumDefaultedMember"));
            }
          }
      }
      if (!this.match(8)) {
        this.expect(12);
      }
    }
    return {
      members,
      hasUnknownMembers
    };
  }
  flowEnumStringMembers(initializedMembers, defaultedMembers, {
    enumName
  }) {
    if (initializedMembers.length === 0) {
      return defaultedMembers;
    } else if (defaultedMembers.length === 0) {
      return initializedMembers;
    } else if (defaultedMembers.length > initializedMembers.length) {
      for (const member of initializedMembers) {
        this.flowEnumErrorStringMemberInconsistentlyInitailized(member, {
          enumName
        });
      }
      return defaultedMembers;
    } else {
      for (const member of defaultedMembers) {
        this.flowEnumErrorStringMemberInconsistentlyInitailized(member, {
          enumName
        });
      }
      return initializedMembers;
    }
  }
  flowEnumParseExplicitType({
    enumName
  }) {
    if (!this.eatContextual(101)) return null;
    if (!(0, _types.tokenIsIdentifier)(this.state.type)) {
      throw this.raise(FlowErrors.EnumInvalidExplicitTypeUnknownSupplied, {
        at: this.state.startLoc,
        enumName
      });
    }
    const {
      value
    } = this.state;
    this.next();
    if (value !== "boolean" && value !== "number" && value !== "string" && value !== "symbol") {
      this.raise(FlowErrors.EnumInvalidExplicitType, {
        at: this.state.startLoc,
        enumName,
        invalidEnumType: value
      });
    }
    return value;
  }
  flowEnumBody(node, id) {
    const enumName = id.name;
    const nameLoc = id.loc.start;
    const explicitType = this.flowEnumParseExplicitType({
      enumName
    });
    this.expect(5);
    const {
      members,
      hasUnknownMembers
    } = this.flowEnumMembers({
      enumName,
      explicitType
    });
    node.hasUnknownMembers = hasUnknownMembers;
    switch (explicitType) {
      case "boolean":
        node.explicitType = true;
        node.members = members.booleanMembers;
        this.expect(8);
        return this.finishNode(node, "EnumBooleanBody");
      case "number":
        node.explicitType = true;
        node.members = members.numberMembers;
        this.expect(8);
        return this.finishNode(node, "EnumNumberBody");
      case "string":
        node.explicitType = true;
        node.members = this.flowEnumStringMembers(members.stringMembers, members.defaultedMembers, {
          enumName
        });
        this.expect(8);
        return this.finishNode(node, "EnumStringBody");
      case "symbol":
        node.members = members.defaultedMembers;
        this.expect(8);
        return this.finishNode(node, "EnumSymbolBody");
      default:
        {
          const empty = () => {
            node.members = [];
            this.expect(8);
            return this.finishNode(node, "EnumStringBody");
          };
          node.explicitType = false;
          const boolsLen = members.booleanMembers.length;
          const numsLen = members.numberMembers.length;
          const strsLen = members.stringMembers.length;
          const defaultedLen = members.defaultedMembers.length;
          if (!boolsLen && !numsLen && !strsLen && !defaultedLen) {
            return empty();
          } else if (!boolsLen && !numsLen) {
            node.members = this.flowEnumStringMembers(members.stringMembers, members.defaultedMembers, {
              enumName
            });
            this.expect(8);
            return this.finishNode(node, "EnumStringBody");
          } else if (!numsLen && !strsLen && boolsLen >= defaultedLen) {
            for (const member of members.defaultedMembers) {
              this.flowEnumErrorBooleanMemberNotInitialized(member.loc.start, {
                enumName,
                memberName: member.id.name
              });
            }
            node.members = members.booleanMembers;
            this.expect(8);
            return this.finishNode(node, "EnumBooleanBody");
          } else if (!boolsLen && !strsLen && numsLen >= defaultedLen) {
            for (const member of members.defaultedMembers) {
              this.flowEnumErrorNumberMemberNotInitialized(member.loc.start, {
                enumName,
                memberName: member.id.name
              });
            }
            node.members = members.numberMembers;
            this.expect(8);
            return this.finishNode(node, "EnumNumberBody");
          } else {
            this.raise(FlowErrors.EnumInconsistentMemberValues, {
              at: nameLoc,
              enumName
            });
            return empty();
          }
        }
    }
  }
  flowParseEnumDeclaration(node) {
    const id = this.parseIdentifier();
    node.id = id;
    node.body = this.flowEnumBody(this.startNode(), id);
    return this.finishNode(node, "EnumDeclaration");
  }
  isLookaheadToken_lt() {
    const next = this.nextTokenStart();
    if (this.input.charCodeAt(next) === 60) {
      const afterNext = this.input.charCodeAt(next + 1);
      return afterNext !== 60 && afterNext !== 61;
    }
    return false;
  }
  maybeUnwrapTypeCastExpression(node) {
    return node.type === "TypeCastExpression" ? node.expression : node;
  }
};
exports.default = _default;

//# sourceMappingURL=index.js.map
