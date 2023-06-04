"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;
var _types = require("../../tokenizer/types");
var _context = require("../../tokenizer/context");
var _location = require("../../util/location");
var _scopeflags = require("../../util/scopeflags");
var _scope = require("./scope");
var _productionParameter = require("../../util/production-parameter");
var _parseError = require("../../parse-error");
var _node = require("../../parser/node");
var _lval = require("../../parser/lval");
const getOwn = (object, key) => Object.hasOwnProperty.call(object, key) && object[key];
function nonNull(x) {
  if (x == null) {
    throw new Error(`Unexpected ${x} value.`);
  }
  return x;
}
function assert(x) {
  if (!x) {
    throw new Error("Assert fail");
  }
}
const TSErrors = (0, _parseError.ParseErrorEnum)`typescript`({
  AbstractMethodHasImplementation: ({
    methodName
  }) => `Method '${methodName}' cannot have an implementation because it is marked abstract.`,
  AbstractPropertyHasInitializer: ({
    propertyName
  }) => `Property '${propertyName}' cannot have an initializer because it is marked abstract.`,
  AccesorCannotDeclareThisParameter: "'get' and 'set' accessors cannot declare 'this' parameters.",
  AccesorCannotHaveTypeParameters: "An accessor cannot have type parameters.",
  AccessorCannotBeOptional: "An 'accessor' property cannot be declared optional.",
  ClassMethodHasDeclare: "Class methods cannot have the 'declare' modifier.",
  ClassMethodHasReadonly: "Class methods cannot have the 'readonly' modifier.",
  ConstInitiailizerMustBeStringOrNumericLiteralOrLiteralEnumReference: "A 'const' initializer in an ambient context must be a string or numeric literal or literal enum reference.",
  ConstructorHasTypeParameters: "Type parameters cannot appear on a constructor declaration.",
  DeclareAccessor: ({
    kind
  }) => `'declare' is not allowed in ${kind}ters.`,
  DeclareClassFieldHasInitializer: "Initializers are not allowed in ambient contexts.",
  DeclareFunctionHasImplementation: "An implementation cannot be declared in ambient contexts.",
  DuplicateAccessibilityModifier: ({
    modifier
  }) => `Accessibility modifier already seen.`,
  DuplicateModifier: ({
    modifier
  }) => `Duplicate modifier: '${modifier}'.`,
  EmptyHeritageClauseType: ({
    token
  }) => `'${token}' list cannot be empty.`,
  EmptyTypeArguments: "Type argument list cannot be empty.",
  EmptyTypeParameters: "Type parameter list cannot be empty.",
  ExpectedAmbientAfterExportDeclare: "'export declare' must be followed by an ambient declaration.",
  ImportAliasHasImportType: "An import alias can not use 'import type'.",
  ImportReflectionHasImportType: "An `import module` declaration can not use `type` modifier",
  IncompatibleModifiers: ({
    modifiers
  }) => `'${modifiers[0]}' modifier cannot be used with '${modifiers[1]}' modifier.`,
  IndexSignatureHasAbstract: "Index signatures cannot have the 'abstract' modifier.",
  IndexSignatureHasAccessibility: ({
    modifier
  }) => `Index signatures cannot have an accessibility modifier ('${modifier}').`,
  IndexSignatureHasDeclare: "Index signatures cannot have the 'declare' modifier.",
  IndexSignatureHasOverride: "'override' modifier cannot appear on an index signature.",
  IndexSignatureHasStatic: "Index signatures cannot have the 'static' modifier.",
  InitializerNotAllowedInAmbientContext: "Initializers are not allowed in ambient contexts.",
  InvalidModifierOnTypeMember: ({
    modifier
  }) => `'${modifier}' modifier cannot appear on a type member.`,
  InvalidModifierOnTypeParameter: ({
    modifier
  }) => `'${modifier}' modifier cannot appear on a type parameter.`,
  InvalidModifierOnTypeParameterPositions: ({
    modifier
  }) => `'${modifier}' modifier can only appear on a type parameter of a class, interface or type alias.`,
  InvalidModifiersOrder: ({
    orderedModifiers
  }) => `'${orderedModifiers[0]}' modifier must precede '${orderedModifiers[1]}' modifier.`,
  InvalidPropertyAccessAfterInstantiationExpression: "Invalid property access after an instantiation expression. " + "You can either wrap the instantiation expression in parentheses, or delete the type arguments.",
  InvalidTupleMemberLabel: "Tuple members must be labeled with a simple identifier.",
  MissingInterfaceName: "'interface' declarations must be followed by an identifier.",
  MixedLabeledAndUnlabeledElements: "Tuple members must all have names or all not have names.",
  NonAbstractClassHasAbstractMethod: "Abstract methods can only appear within an abstract class.",
  NonClassMethodPropertyHasAbstractModifer: "'abstract' modifier can only appear on a class, method, or property declaration.",
  OptionalTypeBeforeRequired: "A required element cannot follow an optional element.",
  OverrideNotInSubClass: "This member cannot have an 'override' modifier because its containing class does not extend another class.",
  PatternIsOptional: "A binding pattern parameter cannot be optional in an implementation signature.",
  PrivateElementHasAbstract: "Private elements cannot have the 'abstract' modifier.",
  PrivateElementHasAccessibility: ({
    modifier
  }) => `Private elements cannot have an accessibility modifier ('${modifier}').`,
  ReadonlyForMethodSignature: "'readonly' modifier can only appear on a property declaration or index signature.",
  ReservedArrowTypeParam: "This syntax is reserved in files with the .mts or .cts extension. Add a trailing comma, as in `<T,>() => ...`.",
  ReservedTypeAssertion: "This syntax is reserved in files with the .mts or .cts extension. Use an `as` expression instead.",
  SetAccesorCannotHaveOptionalParameter: "A 'set' accessor cannot have an optional parameter.",
  SetAccesorCannotHaveRestParameter: "A 'set' accessor cannot have rest parameter.",
  SetAccesorCannotHaveReturnType: "A 'set' accessor cannot have a return type annotation.",
  SingleTypeParameterWithoutTrailingComma: ({
    typeParameterName
  }) => `Single type parameter ${typeParameterName} should have a trailing comma. Example usage: <${typeParameterName},>.`,
  StaticBlockCannotHaveModifier: "Static class blocks cannot have any modifier.",
  TupleOptionalAfterType: "A labeled tuple optional element must be declared using a question mark after the name and before the colon (`name?: type`), rather than after the type (`name: type?`).",
  TypeAnnotationAfterAssign: "Type annotations must come before default assignments, e.g. instead of `age = 25: number` use `age: number = 25`.",
  TypeImportCannotSpecifyDefaultAndNamed: "A type-only import can specify a default import or named bindings, but not both.",
  TypeModifierIsUsedInTypeExports: "The 'type' modifier cannot be used on a named export when 'export type' is used on its export statement.",
  TypeModifierIsUsedInTypeImports: "The 'type' modifier cannot be used on a named import when 'import type' is used on its import statement.",
  UnexpectedParameterModifier: "A parameter property is only allowed in a constructor implementation.",
  UnexpectedReadonly: "'readonly' type modifier is only permitted on array and tuple literal types.",
  UnexpectedTypeAnnotation: "Did not expect a type annotation here.",
  UnexpectedTypeCastInParameter: "Unexpected type cast in parameter position.",
  UnsupportedImportTypeArgument: "Argument in a type import must be a string literal.",
  UnsupportedParameterPropertyKind: "A parameter property may not be declared using a binding pattern.",
  UnsupportedSignatureParameterKind: ({
    type
  }) => `Name in a signature must be an Identifier, ObjectPattern or ArrayPattern, instead got ${type}.`
});
function keywordTypeFromName(value) {
  switch (value) {
    case "any":
      return "TSAnyKeyword";
    case "boolean":
      return "TSBooleanKeyword";
    case "bigint":
      return "TSBigIntKeyword";
    case "never":
      return "TSNeverKeyword";
    case "number":
      return "TSNumberKeyword";
    case "object":
      return "TSObjectKeyword";
    case "string":
      return "TSStringKeyword";
    case "symbol":
      return "TSSymbolKeyword";
    case "undefined":
      return "TSUndefinedKeyword";
    case "unknown":
      return "TSUnknownKeyword";
    default:
      return undefined;
  }
}
function tsIsAccessModifier(modifier) {
  return modifier === "private" || modifier === "public" || modifier === "protected";
}
function tsIsVarianceAnnotations(modifier) {
  return modifier === "in" || modifier === "out";
}
var _default = superClass => class TypeScriptParserMixin extends superClass {
  constructor(...args) {
    super(...args);
    this.tsParseInOutModifiers = this.tsParseModifiers.bind(this, {
      allowedModifiers: ["in", "out"],
      disallowedModifiers: ["const", "public", "private", "protected", "readonly", "declare", "abstract", "override"],
      errorTemplate: TSErrors.InvalidModifierOnTypeParameter
    });
    this.tsParseConstModifier = this.tsParseModifiers.bind(this, {
      allowedModifiers: ["const"],
      disallowedModifiers: ["in", "out"],
      errorTemplate: TSErrors.InvalidModifierOnTypeParameterPositions
    });
    this.tsParseInOutConstModifiers = this.tsParseModifiers.bind(this, {
      allowedModifiers: ["in", "out", "const"],
      disallowedModifiers: ["public", "private", "protected", "readonly", "declare", "abstract", "override"],
      errorTemplate: TSErrors.InvalidModifierOnTypeParameter
    });
  }
  getScopeHandler() {
    return _scope.default;
  }
  tsIsIdentifier() {
    return (0, _types.tokenIsIdentifier)(this.state.type);
  }
  tsTokenCanFollowModifier() {
    return (this.match(0) || this.match(5) || this.match(55) || this.match(21) || this.match(136) || this.isLiteralPropertyName()) && !this.hasPrecedingLineBreak();
  }
  tsNextTokenCanFollowModifier() {
    this.next();
    return this.tsTokenCanFollowModifier();
  }
  tsParseModifier(allowedModifiers, stopOnStartOfClassStaticBlock) {
    if (!(0, _types.tokenIsIdentifier)(this.state.type) && this.state.type !== 58 && this.state.type !== 75) {
      return undefined;
    }
    const modifier = this.state.value;
    if (allowedModifiers.indexOf(modifier) !== -1) {
      if (stopOnStartOfClassStaticBlock && this.tsIsStartOfStaticBlocks()) {
        return undefined;
      }
      if (this.tsTryParse(this.tsNextTokenCanFollowModifier.bind(this))) {
        return modifier;
      }
    }
    return undefined;
  }
  tsParseModifiers({
    allowedModifiers,
    disallowedModifiers,
    stopOnStartOfClassStaticBlock,
    errorTemplate = TSErrors.InvalidModifierOnTypeMember
  }, modified) {
    const enforceOrder = (loc, modifier, before, after) => {
      if (modifier === before && modified[after]) {
        this.raise(TSErrors.InvalidModifiersOrder, {
          at: loc,
          orderedModifiers: [before, after]
        });
      }
    };
    const incompatible = (loc, modifier, mod1, mod2) => {
      if (modified[mod1] && modifier === mod2 || modified[mod2] && modifier === mod1) {
        this.raise(TSErrors.IncompatibleModifiers, {
          at: loc,
          modifiers: [mod1, mod2]
        });
      }
    };
    for (;;) {
      const {
        startLoc
      } = this.state;
      const modifier = this.tsParseModifier(allowedModifiers.concat(disallowedModifiers != null ? disallowedModifiers : []), stopOnStartOfClassStaticBlock);
      if (!modifier) break;
      if (tsIsAccessModifier(modifier)) {
        if (modified.accessibility) {
          this.raise(TSErrors.DuplicateAccessibilityModifier, {
            at: startLoc,
            modifier
          });
        } else {
          enforceOrder(startLoc, modifier, modifier, "override");
          enforceOrder(startLoc, modifier, modifier, "static");
          enforceOrder(startLoc, modifier, modifier, "readonly");
          modified.accessibility = modifier;
        }
      } else if (tsIsVarianceAnnotations(modifier)) {
        if (modified[modifier]) {
          this.raise(TSErrors.DuplicateModifier, {
            at: startLoc,
            modifier
          });
        }
        modified[modifier] = true;
        enforceOrder(startLoc, modifier, "in", "out");
      } else {
        if (Object.hasOwnProperty.call(modified, modifier)) {
          this.raise(TSErrors.DuplicateModifier, {
            at: startLoc,
            modifier
          });
        } else {
          enforceOrder(startLoc, modifier, "static", "readonly");
          enforceOrder(startLoc, modifier, "static", "override");
          enforceOrder(startLoc, modifier, "override", "readonly");
          enforceOrder(startLoc, modifier, "abstract", "override");
          incompatible(startLoc, modifier, "declare", "override");
          incompatible(startLoc, modifier, "static", "abstract");
        }
        modified[modifier] = true;
      }
      if (disallowedModifiers != null && disallowedModifiers.includes(modifier)) {
        this.raise(errorTemplate, {
          at: startLoc,
          modifier
        });
      }
    }
  }
  tsIsListTerminator(kind) {
    switch (kind) {
      case "EnumMembers":
      case "TypeMembers":
        return this.match(8);
      case "HeritageClauseElement":
        return this.match(5);
      case "TupleElementTypes":
        return this.match(3);
      case "TypeParametersOrArguments":
        return this.match(48);
    }
  }
  tsParseList(kind, parseElement) {
    const result = [];
    while (!this.tsIsListTerminator(kind)) {
      result.push(parseElement());
    }
    return result;
  }
  tsParseDelimitedList(kind, parseElement, refTrailingCommaPos) {
    return nonNull(this.tsParseDelimitedListWorker(kind, parseElement, true, refTrailingCommaPos));
  }
  tsParseDelimitedListWorker(kind, parseElement, expectSuccess, refTrailingCommaPos) {
    const result = [];
    let trailingCommaPos = -1;
    for (;;) {
      if (this.tsIsListTerminator(kind)) {
        break;
      }
      trailingCommaPos = -1;
      const element = parseElement();
      if (element == null) {
        return undefined;
      }
      result.push(element);
      if (this.eat(12)) {
        trailingCommaPos = this.state.lastTokStart;
        continue;
      }
      if (this.tsIsListTerminator(kind)) {
        break;
      }
      if (expectSuccess) {
        this.expect(12);
      }
      return undefined;
    }
    if (refTrailingCommaPos) {
      refTrailingCommaPos.value = trailingCommaPos;
    }
    return result;
  }
  tsParseBracketedList(kind, parseElement, bracket, skipFirstToken, refTrailingCommaPos) {
    if (!skipFirstToken) {
      if (bracket) {
        this.expect(0);
      } else {
        this.expect(47);
      }
    }
    const result = this.tsParseDelimitedList(kind, parseElement, refTrailingCommaPos);
    if (bracket) {
      this.expect(3);
    } else {
      this.expect(48);
    }
    return result;
  }
  tsParseImportType() {
    const node = this.startNode();
    this.expect(83);
    this.expect(10);
    if (!this.match(131)) {
      this.raise(TSErrors.UnsupportedImportTypeArgument, {
        at: this.state.startLoc
      });
    }
    node.argument = super.parseExprAtom();
    this.expect(11);
    if (this.eat(16)) {
      node.qualifier = this.tsParseEntityName();
    }
    if (this.match(47)) {
      node.typeParameters = this.tsParseTypeArguments();
    }
    return this.finishNode(node, "TSImportType");
  }
  tsParseEntityName(allowReservedWords = true) {
    let entity = this.parseIdentifier(allowReservedWords);
    while (this.eat(16)) {
      const node = this.startNodeAtNode(entity);
      node.left = entity;
      node.right = this.parseIdentifier(allowReservedWords);
      entity = this.finishNode(node, "TSQualifiedName");
    }
    return entity;
  }
  tsParseTypeReference() {
    const node = this.startNode();
    node.typeName = this.tsParseEntityName();
    if (!this.hasPrecedingLineBreak() && this.match(47)) {
      node.typeParameters = this.tsParseTypeArguments();
    }
    return this.finishNode(node, "TSTypeReference");
  }
  tsParseThisTypePredicate(lhs) {
    this.next();
    const node = this.startNodeAtNode(lhs);
    node.parameterName = lhs;
    node.typeAnnotation = this.tsParseTypeAnnotation(false);
    node.asserts = false;
    return this.finishNode(node, "TSTypePredicate");
  }
  tsParseThisTypeNode() {
    const node = this.startNode();
    this.next();
    return this.finishNode(node, "TSThisType");
  }
  tsParseTypeQuery() {
    const node = this.startNode();
    this.expect(87);
    if (this.match(83)) {
      node.exprName = this.tsParseImportType();
    } else {
      node.exprName = this.tsParseEntityName();
    }
    if (!this.hasPrecedingLineBreak() && this.match(47)) {
      node.typeParameters = this.tsParseTypeArguments();
    }
    return this.finishNode(node, "TSTypeQuery");
  }
  tsParseTypeParameter(parseModifiers) {
    const node = this.startNode();
    parseModifiers(node);
    node.name = this.tsParseTypeParameterName();
    node.constraint = this.tsEatThenParseType(81);
    node.default = this.tsEatThenParseType(29);
    return this.finishNode(node, "TSTypeParameter");
  }
  tsTryParseTypeParameters(parseModifiers) {
    if (this.match(47)) {
      return this.tsParseTypeParameters(parseModifiers);
    }
  }
  tsParseTypeParameters(parseModifiers) {
    const node = this.startNode();
    if (this.match(47) || this.match(140)) {
      this.next();
    } else {
      this.unexpected();
    }
    const refTrailingCommaPos = {
      value: -1
    };
    node.params = this.tsParseBracketedList("TypeParametersOrArguments", this.tsParseTypeParameter.bind(this, parseModifiers), false, true, refTrailingCommaPos);
    if (node.params.length === 0) {
      this.raise(TSErrors.EmptyTypeParameters, {
        at: node
      });
    }
    if (refTrailingCommaPos.value !== -1) {
      this.addExtra(node, "trailingComma", refTrailingCommaPos.value);
    }
    return this.finishNode(node, "TSTypeParameterDeclaration");
  }
  tsFillSignature(returnToken, signature) {
    const returnTokenRequired = returnToken === 19;
    const paramsKey = "parameters";
    const returnTypeKey = "typeAnnotation";
    signature.typeParameters = this.tsTryParseTypeParameters(this.tsParseConstModifier);
    this.expect(10);
    signature[paramsKey] = this.tsParseBindingListForSignature();
    if (returnTokenRequired) {
      signature[returnTypeKey] = this.tsParseTypeOrTypePredicateAnnotation(returnToken);
    } else if (this.match(returnToken)) {
      signature[returnTypeKey] = this.tsParseTypeOrTypePredicateAnnotation(returnToken);
    }
  }
  tsParseBindingListForSignature() {
    const list = super.parseBindingList(11, 41, _lval.ParseBindingListFlags.IS_FUNCTION_PARAMS);
    for (const pattern of list) {
      const {
        type
      } = pattern;
      if (type === "AssignmentPattern" || type === "TSParameterProperty") {
        this.raise(TSErrors.UnsupportedSignatureParameterKind, {
          at: pattern,
          type
        });
      }
    }
    return list;
  }
  tsParseTypeMemberSemicolon() {
    if (!this.eat(12) && !this.isLineTerminator()) {
      this.expect(13);
    }
  }
  tsParseSignatureMember(kind, node) {
    this.tsFillSignature(14, node);
    this.tsParseTypeMemberSemicolon();
    return this.finishNode(node, kind);
  }
  tsIsUnambiguouslyIndexSignature() {
    this.next();
    if ((0, _types.tokenIsIdentifier)(this.state.type)) {
      this.next();
      return this.match(14);
    }
    return false;
  }
  tsTryParseIndexSignature(node) {
    if (!(this.match(0) && this.tsLookAhead(this.tsIsUnambiguouslyIndexSignature.bind(this)))) {
      return;
    }
    this.expect(0);
    const id = this.parseIdentifier();
    id.typeAnnotation = this.tsParseTypeAnnotation();
    this.resetEndLocation(id);
    this.expect(3);
    node.parameters = [id];
    const type = this.tsTryParseTypeAnnotation();
    if (type) node.typeAnnotation = type;
    this.tsParseTypeMemberSemicolon();
    return this.finishNode(node, "TSIndexSignature");
  }
  tsParsePropertyOrMethodSignature(node, readonly) {
    if (this.eat(17)) node.optional = true;
    const nodeAny = node;
    if (this.match(10) || this.match(47)) {
      if (readonly) {
        this.raise(TSErrors.ReadonlyForMethodSignature, {
          at: node
        });
      }
      const method = nodeAny;
      if (method.kind && this.match(47)) {
        this.raise(TSErrors.AccesorCannotHaveTypeParameters, {
          at: this.state.curPosition()
        });
      }
      this.tsFillSignature(14, method);
      this.tsParseTypeMemberSemicolon();
      const paramsKey = "parameters";
      const returnTypeKey = "typeAnnotation";
      if (method.kind === "get") {
        if (method[paramsKey].length > 0) {
          this.raise(_parseError.Errors.BadGetterArity, {
            at: this.state.curPosition()
          });
          if (this.isThisParam(method[paramsKey][0])) {
            this.raise(TSErrors.AccesorCannotDeclareThisParameter, {
              at: this.state.curPosition()
            });
          }
        }
      } else if (method.kind === "set") {
        if (method[paramsKey].length !== 1) {
          this.raise(_parseError.Errors.BadSetterArity, {
            at: this.state.curPosition()
          });
        } else {
          const firstParameter = method[paramsKey][0];
          if (this.isThisParam(firstParameter)) {
            this.raise(TSErrors.AccesorCannotDeclareThisParameter, {
              at: this.state.curPosition()
            });
          }
          if (firstParameter.type === "Identifier" && firstParameter.optional) {
            this.raise(TSErrors.SetAccesorCannotHaveOptionalParameter, {
              at: this.state.curPosition()
            });
          }
          if (firstParameter.type === "RestElement") {
            this.raise(TSErrors.SetAccesorCannotHaveRestParameter, {
              at: this.state.curPosition()
            });
          }
        }
        if (method[returnTypeKey]) {
          this.raise(TSErrors.SetAccesorCannotHaveReturnType, {
            at: method[returnTypeKey]
          });
        }
      } else {
        method.kind = "method";
      }
      return this.finishNode(method, "TSMethodSignature");
    } else {
      const property = nodeAny;
      if (readonly) property.readonly = true;
      const type = this.tsTryParseTypeAnnotation();
      if (type) property.typeAnnotation = type;
      this.tsParseTypeMemberSemicolon();
      return this.finishNode(property, "TSPropertySignature");
    }
  }
  tsParseTypeMember() {
    const node = this.startNode();
    if (this.match(10) || this.match(47)) {
      return this.tsParseSignatureMember("TSCallSignatureDeclaration", node);
    }
    if (this.match(77)) {
      const id = this.startNode();
      this.next();
      if (this.match(10) || this.match(47)) {
        return this.tsParseSignatureMember("TSConstructSignatureDeclaration", node);
      } else {
        node.key = this.createIdentifier(id, "new");
        return this.tsParsePropertyOrMethodSignature(node, false);
      }
    }
    this.tsParseModifiers({
      allowedModifiers: ["readonly"],
      disallowedModifiers: ["declare", "abstract", "private", "protected", "public", "static", "override"]
    }, node);
    const idx = this.tsTryParseIndexSignature(node);
    if (idx) {
      return idx;
    }
    super.parsePropertyName(node);
    if (!node.computed && node.key.type === "Identifier" && (node.key.name === "get" || node.key.name === "set") && this.tsTokenCanFollowModifier()) {
      node.kind = node.key.name;
      super.parsePropertyName(node);
    }
    return this.tsParsePropertyOrMethodSignature(node, !!node.readonly);
  }
  tsParseTypeLiteral() {
    const node = this.startNode();
    node.members = this.tsParseObjectTypeMembers();
    return this.finishNode(node, "TSTypeLiteral");
  }
  tsParseObjectTypeMembers() {
    this.expect(5);
    const members = this.tsParseList("TypeMembers", this.tsParseTypeMember.bind(this));
    this.expect(8);
    return members;
  }
  tsIsStartOfMappedType() {
    this.next();
    if (this.eat(53)) {
      return this.isContextual(120);
    }
    if (this.isContextual(120)) {
      this.next();
    }
    if (!this.match(0)) {
      return false;
    }
    this.next();
    if (!this.tsIsIdentifier()) {
      return false;
    }
    this.next();
    return this.match(58);
  }
  tsParseMappedTypeParameter() {
    const node = this.startNode();
    node.name = this.tsParseTypeParameterName();
    node.constraint = this.tsExpectThenParseType(58);
    return this.finishNode(node, "TSTypeParameter");
  }
  tsParseMappedType() {
    const node = this.startNode();
    this.expect(5);
    if (this.match(53)) {
      node.readonly = this.state.value;
      this.next();
      this.expectContextual(120);
    } else if (this.eatContextual(120)) {
      node.readonly = true;
    }
    this.expect(0);
    node.typeParameter = this.tsParseMappedTypeParameter();
    node.nameType = this.eatContextual(93) ? this.tsParseType() : null;
    this.expect(3);
    if (this.match(53)) {
      node.optional = this.state.value;
      this.next();
      this.expect(17);
    } else if (this.eat(17)) {
      node.optional = true;
    }
    node.typeAnnotation = this.tsTryParseType();
    this.semicolon();
    this.expect(8);
    return this.finishNode(node, "TSMappedType");
  }
  tsParseTupleType() {
    const node = this.startNode();
    node.elementTypes = this.tsParseBracketedList("TupleElementTypes", this.tsParseTupleElementType.bind(this), true, false);
    let seenOptionalElement = false;
    let labeledElements = null;
    node.elementTypes.forEach(elementNode => {
      var _labeledElements;
      const {
        type
      } = elementNode;
      if (seenOptionalElement && type !== "TSRestType" && type !== "TSOptionalType" && !(type === "TSNamedTupleMember" && elementNode.optional)) {
        this.raise(TSErrors.OptionalTypeBeforeRequired, {
          at: elementNode
        });
      }
      seenOptionalElement || (seenOptionalElement = type === "TSNamedTupleMember" && elementNode.optional || type === "TSOptionalType");
      let checkType = type;
      if (type === "TSRestType") {
        elementNode = elementNode.typeAnnotation;
        checkType = elementNode.type;
      }
      const isLabeled = checkType === "TSNamedTupleMember";
      (_labeledElements = labeledElements) != null ? _labeledElements : labeledElements = isLabeled;
      if (labeledElements !== isLabeled) {
        this.raise(TSErrors.MixedLabeledAndUnlabeledElements, {
          at: elementNode
        });
      }
    });
    return this.finishNode(node, "TSTupleType");
  }
  tsParseTupleElementType() {
    const {
      startLoc
    } = this.state;
    const rest = this.eat(21);
    let labeled;
    let label;
    let optional;
    let type;
    const isWord = (0, _types.tokenIsKeywordOrIdentifier)(this.state.type);
    const chAfterWord = isWord ? this.lookaheadCharCode() : null;
    if (chAfterWord === 58) {
      labeled = true;
      optional = false;
      label = this.parseIdentifier(true);
      this.expect(14);
      type = this.tsParseType();
    } else if (chAfterWord === 63) {
      optional = true;
      const startLoc = this.state.startLoc;
      const wordName = this.state.value;
      const typeOrLabel = this.tsParseNonArrayType();
      if (this.lookaheadCharCode() === 58) {
        labeled = true;
        label = this.createIdentifier(this.startNodeAt(startLoc), wordName);
        this.expect(17);
        this.expect(14);
        type = this.tsParseType();
      } else {
        labeled = false;
        type = typeOrLabel;
        this.expect(17);
      }
    } else {
      type = this.tsParseType();
      optional = this.eat(17);
      labeled = this.eat(14);
    }
    if (labeled) {
      let labeledNode;
      if (label) {
        labeledNode = this.startNodeAtNode(label);
        labeledNode.optional = optional;
        labeledNode.label = label;
        labeledNode.elementType = type;
        if (this.eat(17)) {
          labeledNode.optional = true;
          this.raise(TSErrors.TupleOptionalAfterType, {
            at: this.state.lastTokStartLoc
          });
        }
      } else {
        labeledNode = this.startNodeAtNode(type);
        labeledNode.optional = optional;
        this.raise(TSErrors.InvalidTupleMemberLabel, {
          at: type
        });
        labeledNode.label = type;
        labeledNode.elementType = this.tsParseType();
      }
      type = this.finishNode(labeledNode, "TSNamedTupleMember");
    } else if (optional) {
      const optionalTypeNode = this.startNodeAtNode(type);
      optionalTypeNode.typeAnnotation = type;
      type = this.finishNode(optionalTypeNode, "TSOptionalType");
    }
    if (rest) {
      const restNode = this.startNodeAt(startLoc);
      restNode.typeAnnotation = type;
      type = this.finishNode(restNode, "TSRestType");
    }
    return type;
  }
  tsParseParenthesizedType() {
    const node = this.startNode();
    this.expect(10);
    node.typeAnnotation = this.tsParseType();
    this.expect(11);
    return this.finishNode(node, "TSParenthesizedType");
  }
  tsParseFunctionOrConstructorType(type, abstract) {
    const node = this.startNode();
    if (type === "TSConstructorType") {
      node.abstract = !!abstract;
      if (abstract) this.next();
      this.next();
    }
    this.tsInAllowConditionalTypesContext(() => this.tsFillSignature(19, node));
    return this.finishNode(node, type);
  }
  tsParseLiteralTypeNode() {
    const node = this.startNode();
    switch (this.state.type) {
      case 132:
      case 133:
      case 131:
      case 85:
      case 86:
        node.literal = super.parseExprAtom();
        break;
      default:
        this.unexpected();
    }
    return this.finishNode(node, "TSLiteralType");
  }
  tsParseTemplateLiteralType() {
    const node = this.startNode();
    node.literal = super.parseTemplate(false);
    return this.finishNode(node, "TSLiteralType");
  }
  parseTemplateSubstitution() {
    if (this.state.inType) return this.tsParseType();
    return super.parseTemplateSubstitution();
  }
  tsParseThisTypeOrThisTypePredicate() {
    const thisKeyword = this.tsParseThisTypeNode();
    if (this.isContextual(114) && !this.hasPrecedingLineBreak()) {
      return this.tsParseThisTypePredicate(thisKeyword);
    } else {
      return thisKeyword;
    }
  }
  tsParseNonArrayType() {
    switch (this.state.type) {
      case 131:
      case 132:
      case 133:
      case 85:
      case 86:
        return this.tsParseLiteralTypeNode();
      case 53:
        if (this.state.value === "-") {
          const node = this.startNode();
          const nextToken = this.lookahead();
          if (nextToken.type !== 132 && nextToken.type !== 133) {
            this.unexpected();
          }
          node.literal = this.parseMaybeUnary();
          return this.finishNode(node, "TSLiteralType");
        }
        break;
      case 78:
        return this.tsParseThisTypeOrThisTypePredicate();
      case 87:
        return this.tsParseTypeQuery();
      case 83:
        return this.tsParseImportType();
      case 5:
        return this.tsLookAhead(this.tsIsStartOfMappedType.bind(this)) ? this.tsParseMappedType() : this.tsParseTypeLiteral();
      case 0:
        return this.tsParseTupleType();
      case 10:
        ;
        return this.tsParseParenthesizedType();
      case 25:
      case 24:
        return this.tsParseTemplateLiteralType();
      default:
        {
          const {
            type
          } = this.state;
          if ((0, _types.tokenIsIdentifier)(type) || type === 88 || type === 84) {
            const nodeType = type === 88 ? "TSVoidKeyword" : type === 84 ? "TSNullKeyword" : keywordTypeFromName(this.state.value);
            if (nodeType !== undefined && this.lookaheadCharCode() !== 46) {
              const node = this.startNode();
              this.next();
              return this.finishNode(node, nodeType);
            }
            return this.tsParseTypeReference();
          }
        }
    }
    this.unexpected();
  }
  tsParseArrayTypeOrHigher() {
    let type = this.tsParseNonArrayType();
    while (!this.hasPrecedingLineBreak() && this.eat(0)) {
      if (this.match(3)) {
        const node = this.startNodeAtNode(type);
        node.elementType = type;
        this.expect(3);
        type = this.finishNode(node, "TSArrayType");
      } else {
        const node = this.startNodeAtNode(type);
        node.objectType = type;
        node.indexType = this.tsParseType();
        this.expect(3);
        type = this.finishNode(node, "TSIndexedAccessType");
      }
    }
    return type;
  }
  tsParseTypeOperator() {
    const node = this.startNode();
    const operator = this.state.value;
    this.next();
    node.operator = operator;
    node.typeAnnotation = this.tsParseTypeOperatorOrHigher();
    if (operator === "readonly") {
      this.tsCheckTypeAnnotationForReadOnly(node);
    }
    return this.finishNode(node, "TSTypeOperator");
  }
  tsCheckTypeAnnotationForReadOnly(node) {
    switch (node.typeAnnotation.type) {
      case "TSTupleType":
      case "TSArrayType":
        return;
      default:
        this.raise(TSErrors.UnexpectedReadonly, {
          at: node
        });
    }
  }
  tsParseInferType() {
    const node = this.startNode();
    this.expectContextual(113);
    const typeParameter = this.startNode();
    typeParameter.name = this.tsParseTypeParameterName();
    typeParameter.constraint = this.tsTryParse(() => this.tsParseConstraintForInferType());
    node.typeParameter = this.finishNode(typeParameter, "TSTypeParameter");
    return this.finishNode(node, "TSInferType");
  }
  tsParseConstraintForInferType() {
    if (this.eat(81)) {
      const constraint = this.tsInDisallowConditionalTypesContext(() => this.tsParseType());
      if (this.state.inDisallowConditionalTypesContext || !this.match(17)) {
        return constraint;
      }
    }
  }
  tsParseTypeOperatorOrHigher() {
    const isTypeOperator = (0, _types.tokenIsTSTypeOperator)(this.state.type) && !this.state.containsEsc;
    return isTypeOperator ? this.tsParseTypeOperator() : this.isContextual(113) ? this.tsParseInferType() : this.tsInAllowConditionalTypesContext(() => this.tsParseArrayTypeOrHigher());
  }
  tsParseUnionOrIntersectionType(kind, parseConstituentType, operator) {
    const node = this.startNode();
    const hasLeadingOperator = this.eat(operator);
    const types = [];
    do {
      types.push(parseConstituentType());
    } while (this.eat(operator));
    if (types.length === 1 && !hasLeadingOperator) {
      return types[0];
    }
    node.types = types;
    return this.finishNode(node, kind);
  }
  tsParseIntersectionTypeOrHigher() {
    return this.tsParseUnionOrIntersectionType("TSIntersectionType", this.tsParseTypeOperatorOrHigher.bind(this), 45);
  }
  tsParseUnionTypeOrHigher() {
    return this.tsParseUnionOrIntersectionType("TSUnionType", this.tsParseIntersectionTypeOrHigher.bind(this), 43);
  }
  tsIsStartOfFunctionType() {
    if (this.match(47)) {
      return true;
    }
    return this.match(10) && this.tsLookAhead(this.tsIsUnambiguouslyStartOfFunctionType.bind(this));
  }
  tsSkipParameterStart() {
    if ((0, _types.tokenIsIdentifier)(this.state.type) || this.match(78)) {
      this.next();
      return true;
    }
    if (this.match(5)) {
      const {
        errors
      } = this.state;
      const previousErrorCount = errors.length;
      try {
        this.parseObjectLike(8, true);
        return errors.length === previousErrorCount;
      } catch (_unused) {
        return false;
      }
    }
    if (this.match(0)) {
      this.next();
      const {
        errors
      } = this.state;
      const previousErrorCount = errors.length;
      try {
        super.parseBindingList(3, 93, _lval.ParseBindingListFlags.ALLOW_EMPTY);
        return errors.length === previousErrorCount;
      } catch (_unused2) {
        return false;
      }
    }
    return false;
  }
  tsIsUnambiguouslyStartOfFunctionType() {
    this.next();
    if (this.match(11) || this.match(21)) {
      return true;
    }
    if (this.tsSkipParameterStart()) {
      if (this.match(14) || this.match(12) || this.match(17) || this.match(29)) {
        return true;
      }
      if (this.match(11)) {
        this.next();
        if (this.match(19)) {
          return true;
        }
      }
    }
    return false;
  }
  tsParseTypeOrTypePredicateAnnotation(returnToken) {
    return this.tsInType(() => {
      const t = this.startNode();
      this.expect(returnToken);
      const node = this.startNode();
      const asserts = !!this.tsTryParse(this.tsParseTypePredicateAsserts.bind(this));
      if (asserts && this.match(78)) {
        let thisTypePredicate = this.tsParseThisTypeOrThisTypePredicate();
        if (thisTypePredicate.type === "TSThisType") {
          node.parameterName = thisTypePredicate;
          node.asserts = true;
          node.typeAnnotation = null;
          thisTypePredicate = this.finishNode(node, "TSTypePredicate");
        } else {
          this.resetStartLocationFromNode(thisTypePredicate, node);
          thisTypePredicate.asserts = true;
        }
        t.typeAnnotation = thisTypePredicate;
        return this.finishNode(t, "TSTypeAnnotation");
      }
      const typePredicateVariable = this.tsIsIdentifier() && this.tsTryParse(this.tsParseTypePredicatePrefix.bind(this));
      if (!typePredicateVariable) {
        if (!asserts) {
          return this.tsParseTypeAnnotation(false, t);
        }
        node.parameterName = this.parseIdentifier();
        node.asserts = asserts;
        node.typeAnnotation = null;
        t.typeAnnotation = this.finishNode(node, "TSTypePredicate");
        return this.finishNode(t, "TSTypeAnnotation");
      }
      const type = this.tsParseTypeAnnotation(false);
      node.parameterName = typePredicateVariable;
      node.typeAnnotation = type;
      node.asserts = asserts;
      t.typeAnnotation = this.finishNode(node, "TSTypePredicate");
      return this.finishNode(t, "TSTypeAnnotation");
    });
  }
  tsTryParseTypeOrTypePredicateAnnotation() {
    if (this.match(14)) {
      return this.tsParseTypeOrTypePredicateAnnotation(14);
    }
  }
  tsTryParseTypeAnnotation() {
    if (this.match(14)) {
      return this.tsParseTypeAnnotation();
    }
  }
  tsTryParseType() {
    return this.tsEatThenParseType(14);
  }
  tsParseTypePredicatePrefix() {
    const id = this.parseIdentifier();
    if (this.isContextual(114) && !this.hasPrecedingLineBreak()) {
      this.next();
      return id;
    }
  }
  tsParseTypePredicateAsserts() {
    if (this.state.type !== 107) {
      return false;
    }
    const containsEsc = this.state.containsEsc;
    this.next();
    if (!(0, _types.tokenIsIdentifier)(this.state.type) && !this.match(78)) {
      return false;
    }
    if (containsEsc) {
      this.raise(_parseError.Errors.InvalidEscapedReservedWord, {
        at: this.state.lastTokStartLoc,
        reservedWord: "asserts"
      });
    }
    return true;
  }
  tsParseTypeAnnotation(eatColon = true, t = this.startNode()) {
    this.tsInType(() => {
      if (eatColon) this.expect(14);
      t.typeAnnotation = this.tsParseType();
    });
    return this.finishNode(t, "TSTypeAnnotation");
  }
  tsParseType() {
    assert(this.state.inType);
    const type = this.tsParseNonConditionalType();
    if (this.state.inDisallowConditionalTypesContext || this.hasPrecedingLineBreak() || !this.eat(81)) {
      return type;
    }
    const node = this.startNodeAtNode(type);
    node.checkType = type;
    node.extendsType = this.tsInDisallowConditionalTypesContext(() => this.tsParseNonConditionalType());
    this.expect(17);
    node.trueType = this.tsInAllowConditionalTypesContext(() => this.tsParseType());
    this.expect(14);
    node.falseType = this.tsInAllowConditionalTypesContext(() => this.tsParseType());
    return this.finishNode(node, "TSConditionalType");
  }
  isAbstractConstructorSignature() {
    return this.isContextual(122) && this.lookahead().type === 77;
  }
  tsParseNonConditionalType() {
    if (this.tsIsStartOfFunctionType()) {
      return this.tsParseFunctionOrConstructorType("TSFunctionType");
    }
    if (this.match(77)) {
      return this.tsParseFunctionOrConstructorType("TSConstructorType");
    } else if (this.isAbstractConstructorSignature()) {
      return this.tsParseFunctionOrConstructorType("TSConstructorType", true);
    }
    return this.tsParseUnionTypeOrHigher();
  }
  tsParseTypeAssertion() {
    if (this.getPluginOption("typescript", "disallowAmbiguousJSXLike")) {
      this.raise(TSErrors.ReservedTypeAssertion, {
        at: this.state.startLoc
      });
    }
    const node = this.startNode();
    node.typeAnnotation = this.tsInType(() => {
      this.next();
      return this.match(75) ? this.tsParseTypeReference() : this.tsParseType();
    });
    this.expect(48);
    node.expression = this.parseMaybeUnary();
    return this.finishNode(node, "TSTypeAssertion");
  }
  tsParseHeritageClause(token) {
    const originalStartLoc = this.state.startLoc;
    const delimitedList = this.tsParseDelimitedList("HeritageClauseElement", () => {
      const node = this.startNode();
      node.expression = this.tsParseEntityName();
      if (this.match(47)) {
        node.typeParameters = this.tsParseTypeArguments();
      }
      return this.finishNode(node, "TSExpressionWithTypeArguments");
    });
    if (!delimitedList.length) {
      this.raise(TSErrors.EmptyHeritageClauseType, {
        at: originalStartLoc,
        token
      });
    }
    return delimitedList;
  }
  tsParseInterfaceDeclaration(node, properties = {}) {
    if (this.hasFollowingLineBreak()) return null;
    this.expectContextual(127);
    if (properties.declare) node.declare = true;
    if ((0, _types.tokenIsIdentifier)(this.state.type)) {
      node.id = this.parseIdentifier();
      this.checkIdentifier(node.id, _scopeflags.BIND_TS_INTERFACE);
    } else {
      node.id = null;
      this.raise(TSErrors.MissingInterfaceName, {
        at: this.state.startLoc
      });
    }
    node.typeParameters = this.tsTryParseTypeParameters(this.tsParseInOutConstModifiers);
    if (this.eat(81)) {
      node.extends = this.tsParseHeritageClause("extends");
    }
    const body = this.startNode();
    body.body = this.tsInType(this.tsParseObjectTypeMembers.bind(this));
    node.body = this.finishNode(body, "TSInterfaceBody");
    return this.finishNode(node, "TSInterfaceDeclaration");
  }
  tsParseTypeAliasDeclaration(node) {
    node.id = this.parseIdentifier();
    this.checkIdentifier(node.id, _scopeflags.BIND_TS_TYPE);
    node.typeAnnotation = this.tsInType(() => {
      node.typeParameters = this.tsTryParseTypeParameters(this.tsParseInOutModifiers);
      this.expect(29);
      if (this.isContextual(112) && this.lookahead().type !== 16) {
        const node = this.startNode();
        this.next();
        return this.finishNode(node, "TSIntrinsicKeyword");
      }
      return this.tsParseType();
    });
    this.semicolon();
    return this.finishNode(node, "TSTypeAliasDeclaration");
  }
  tsInNoContext(cb) {
    const oldContext = this.state.context;
    this.state.context = [oldContext[0]];
    try {
      return cb();
    } finally {
      this.state.context = oldContext;
    }
  }
  tsInType(cb) {
    const oldInType = this.state.inType;
    this.state.inType = true;
    try {
      return cb();
    } finally {
      this.state.inType = oldInType;
    }
  }
  tsInDisallowConditionalTypesContext(cb) {
    const oldInDisallowConditionalTypesContext = this.state.inDisallowConditionalTypesContext;
    this.state.inDisallowConditionalTypesContext = true;
    try {
      return cb();
    } finally {
      this.state.inDisallowConditionalTypesContext = oldInDisallowConditionalTypesContext;
    }
  }
  tsInAllowConditionalTypesContext(cb) {
    const oldInDisallowConditionalTypesContext = this.state.inDisallowConditionalTypesContext;
    this.state.inDisallowConditionalTypesContext = false;
    try {
      return cb();
    } finally {
      this.state.inDisallowConditionalTypesContext = oldInDisallowConditionalTypesContext;
    }
  }
  tsEatThenParseType(token) {
    if (this.match(token)) {
      return this.tsNextThenParseType();
    }
  }
  tsExpectThenParseType(token) {
    return this.tsInType(() => {
      this.expect(token);
      return this.tsParseType();
    });
  }
  tsNextThenParseType() {
    return this.tsInType(() => {
      this.next();
      return this.tsParseType();
    });
  }
  tsParseEnumMember() {
    const node = this.startNode();
    node.id = this.match(131) ? super.parseStringLiteral(this.state.value) : this.parseIdentifier(true);
    if (this.eat(29)) {
      node.initializer = super.parseMaybeAssignAllowIn();
    }
    return this.finishNode(node, "TSEnumMember");
  }
  tsParseEnumDeclaration(node, properties = {}) {
    if (properties.const) node.const = true;
    if (properties.declare) node.declare = true;
    this.expectContextual(124);
    node.id = this.parseIdentifier();
    this.checkIdentifier(node.id, node.const ? _scopeflags.BIND_TS_CONST_ENUM : _scopeflags.BIND_TS_ENUM);
    this.expect(5);
    node.members = this.tsParseDelimitedList("EnumMembers", this.tsParseEnumMember.bind(this));
    this.expect(8);
    return this.finishNode(node, "TSEnumDeclaration");
  }
  tsParseModuleBlock() {
    const node = this.startNode();
    this.scope.enter(_scopeflags.SCOPE_OTHER);
    this.expect(5);
    super.parseBlockOrModuleBlockBody(node.body = [], undefined, true, 8);
    this.scope.exit();
    return this.finishNode(node, "TSModuleBlock");
  }
  tsParseModuleOrNamespaceDeclaration(node, nested = false) {
    node.id = this.parseIdentifier();
    if (!nested) {
      this.checkIdentifier(node.id, _scopeflags.BIND_TS_NAMESPACE);
    }
    if (this.eat(16)) {
      const inner = this.startNode();
      this.tsParseModuleOrNamespaceDeclaration(inner, true);
      node.body = inner;
    } else {
      this.scope.enter(_scopeflags.SCOPE_TS_MODULE);
      this.prodParam.enter(_productionParameter.PARAM);
      node.body = this.tsParseModuleBlock();
      this.prodParam.exit();
      this.scope.exit();
    }
    return this.finishNode(node, "TSModuleDeclaration");
  }
  tsParseAmbientExternalModuleDeclaration(node) {
    if (this.isContextual(110)) {
      node.global = true;
      node.id = this.parseIdentifier();
    } else if (this.match(131)) {
      node.id = super.parseStringLiteral(this.state.value);
    } else {
      this.unexpected();
    }
    if (this.match(5)) {
      this.scope.enter(_scopeflags.SCOPE_TS_MODULE);
      this.prodParam.enter(_productionParameter.PARAM);
      node.body = this.tsParseModuleBlock();
      this.prodParam.exit();
      this.scope.exit();
    } else {
      this.semicolon();
    }
    return this.finishNode(node, "TSModuleDeclaration");
  }
  tsParseImportEqualsDeclaration(node, maybeDefaultIdentifier, isExport) {
    node.isExport = isExport || false;
    node.id = maybeDefaultIdentifier || this.parseIdentifier();
    this.checkIdentifier(node.id, _scopeflags.BIND_FLAGS_TS_IMPORT);
    this.expect(29);
    const moduleReference = this.tsParseModuleReference();
    if (node.importKind === "type" && moduleReference.type !== "TSExternalModuleReference") {
      this.raise(TSErrors.ImportAliasHasImportType, {
        at: moduleReference
      });
    }
    node.moduleReference = moduleReference;
    this.semicolon();
    return this.finishNode(node, "TSImportEqualsDeclaration");
  }
  tsIsExternalModuleReference() {
    return this.isContextual(117) && this.lookaheadCharCode() === 40;
  }
  tsParseModuleReference() {
    return this.tsIsExternalModuleReference() ? this.tsParseExternalModuleReference() : this.tsParseEntityName(false);
  }
  tsParseExternalModuleReference() {
    const node = this.startNode();
    this.expectContextual(117);
    this.expect(10);
    if (!this.match(131)) {
      this.unexpected();
    }
    node.expression = super.parseExprAtom();
    this.expect(11);
    this.sawUnambiguousESM = true;
    return this.finishNode(node, "TSExternalModuleReference");
  }
  tsLookAhead(f) {
    const state = this.state.clone();
    const res = f();
    this.state = state;
    return res;
  }
  tsTryParseAndCatch(f) {
    const result = this.tryParse(abort => f() || abort());
    if (result.aborted || !result.node) return;
    if (result.error) this.state = result.failState;
    return result.node;
  }
  tsTryParse(f) {
    const state = this.state.clone();
    const result = f();
    if (result !== undefined && result !== false) {
      return result;
    }
    this.state = state;
  }
  tsTryParseDeclare(nany) {
    if (this.isLineTerminator()) {
      return;
    }
    let startType = this.state.type;
    let kind;
    if (this.isContextual(99)) {
      startType = 74;
      kind = "let";
    }
    return this.tsInAmbientContext(() => {
      switch (startType) {
        case 68:
          nany.declare = true;
          return super.parseFunctionStatement(nany, false, false);
        case 80:
          nany.declare = true;
          return this.parseClass(nany, true, false);
        case 124:
          return this.tsParseEnumDeclaration(nany, {
            declare: true
          });
        case 110:
          return this.tsParseAmbientExternalModuleDeclaration(nany);
        case 75:
        case 74:
          if (!this.match(75) || !this.isLookaheadContextual("enum")) {
            nany.declare = true;
            return this.parseVarStatement(nany, kind || this.state.value, true);
          }
          this.expect(75);
          return this.tsParseEnumDeclaration(nany, {
            const: true,
            declare: true
          });
        case 127:
          {
            const result = this.tsParseInterfaceDeclaration(nany, {
              declare: true
            });
            if (result) return result;
          }
        default:
          if ((0, _types.tokenIsIdentifier)(startType)) {
            return this.tsParseDeclaration(nany, this.state.value, true, null);
          }
      }
    });
  }
  tsTryParseExportDeclaration() {
    return this.tsParseDeclaration(this.startNode(), this.state.value, true, null);
  }
  tsParseExpressionStatement(node, expr, decorators) {
    switch (expr.name) {
      case "declare":
        {
          const declaration = this.tsTryParseDeclare(node);
          if (declaration) {
            declaration.declare = true;
          }
          return declaration;
        }
      case "global":
        if (this.match(5)) {
          this.scope.enter(_scopeflags.SCOPE_TS_MODULE);
          this.prodParam.enter(_productionParameter.PARAM);
          const mod = node;
          mod.global = true;
          mod.id = expr;
          mod.body = this.tsParseModuleBlock();
          this.scope.exit();
          this.prodParam.exit();
          return this.finishNode(mod, "TSModuleDeclaration");
        }
        break;
      default:
        return this.tsParseDeclaration(node, expr.name, false, decorators);
    }
  }
  tsParseDeclaration(node, value, next, decorators) {
    switch (value) {
      case "abstract":
        if (this.tsCheckLineTerminator(next) && (this.match(80) || (0, _types.tokenIsIdentifier)(this.state.type))) {
          return this.tsParseAbstractDeclaration(node, decorators);
        }
        break;
      case "module":
        if (this.tsCheckLineTerminator(next)) {
          if (this.match(131)) {
            return this.tsParseAmbientExternalModuleDeclaration(node);
          } else if ((0, _types.tokenIsIdentifier)(this.state.type)) {
            return this.tsParseModuleOrNamespaceDeclaration(node);
          }
        }
        break;
      case "namespace":
        if (this.tsCheckLineTerminator(next) && (0, _types.tokenIsIdentifier)(this.state.type)) {
          return this.tsParseModuleOrNamespaceDeclaration(node);
        }
        break;
      case "type":
        if (this.tsCheckLineTerminator(next) && (0, _types.tokenIsIdentifier)(this.state.type)) {
          return this.tsParseTypeAliasDeclaration(node);
        }
        break;
    }
  }
  tsCheckLineTerminator(next) {
    if (next) {
      if (this.hasFollowingLineBreak()) return false;
      this.next();
      return true;
    }
    return !this.isLineTerminator();
  }
  tsTryParseGenericAsyncArrowFunction(startLoc) {
    if (!this.match(47)) return;
    const oldMaybeInArrowParameters = this.state.maybeInArrowParameters;
    this.state.maybeInArrowParameters = true;
    const res = this.tsTryParseAndCatch(() => {
      const node = this.startNodeAt(startLoc);
      node.typeParameters = this.tsParseTypeParameters(this.tsParseConstModifier);
      super.parseFunctionParams(node);
      node.returnType = this.tsTryParseTypeOrTypePredicateAnnotation();
      this.expect(19);
      return node;
    });
    this.state.maybeInArrowParameters = oldMaybeInArrowParameters;
    if (!res) return;
    return super.parseArrowExpression(res, null, true);
  }
  tsParseTypeArgumentsInExpression() {
    if (this.reScan_lt() !== 47) return;
    return this.tsParseTypeArguments();
  }
  tsParseTypeArguments() {
    const node = this.startNode();
    node.params = this.tsInType(() => this.tsInNoContext(() => {
      this.expect(47);
      return this.tsParseDelimitedList("TypeParametersOrArguments", this.tsParseType.bind(this));
    }));
    if (node.params.length === 0) {
      this.raise(TSErrors.EmptyTypeArguments, {
        at: node
      });
    } else if (!this.state.inType && this.curContext() === _context.types.brace) {
      this.reScan_lt_gt();
    }
    this.expect(48);
    return this.finishNode(node, "TSTypeParameterInstantiation");
  }
  tsIsDeclarationStart() {
    return (0, _types.tokenIsTSDeclarationStart)(this.state.type);
  }
  isExportDefaultSpecifier() {
    if (this.tsIsDeclarationStart()) return false;
    return super.isExportDefaultSpecifier();
  }
  parseAssignableListItem(flags, decorators) {
    const startLoc = this.state.startLoc;
    const modified = {};
    this.tsParseModifiers({
      allowedModifiers: ["public", "private", "protected", "override", "readonly"]
    }, modified);
    const accessibility = modified.accessibility;
    const override = modified.override;
    const readonly = modified.readonly;
    if (!(flags & _lval.ParseBindingListFlags.IS_CONSTRUCTOR_PARAMS) && (accessibility || readonly || override)) {
      this.raise(TSErrors.UnexpectedParameterModifier, {
        at: startLoc
      });
    }
    const left = this.parseMaybeDefault();
    this.parseAssignableListItemTypes(left, flags);
    const elt = this.parseMaybeDefault(left.loc.start, left);
    if (accessibility || readonly || override) {
      const pp = this.startNodeAt(startLoc);
      if (decorators.length) {
        pp.decorators = decorators;
      }
      if (accessibility) pp.accessibility = accessibility;
      if (readonly) pp.readonly = readonly;
      if (override) pp.override = override;
      if (elt.type !== "Identifier" && elt.type !== "AssignmentPattern") {
        this.raise(TSErrors.UnsupportedParameterPropertyKind, {
          at: pp
        });
      }
      pp.parameter = elt;
      return this.finishNode(pp, "TSParameterProperty");
    }
    if (decorators.length) {
      left.decorators = decorators;
    }
    return elt;
  }
  isSimpleParameter(node) {
    return node.type === "TSParameterProperty" && super.isSimpleParameter(node.parameter) || super.isSimpleParameter(node);
  }
  tsDisallowOptionalPattern(node) {
    for (const param of node.params) {
      if (param.type !== "Identifier" && param.optional && !this.state.isAmbientContext) {
        this.raise(TSErrors.PatternIsOptional, {
          at: param
        });
      }
    }
  }
  setArrowFunctionParameters(node, params, trailingCommaLoc) {
    super.setArrowFunctionParameters(node, params, trailingCommaLoc);
    this.tsDisallowOptionalPattern(node);
  }
  parseFunctionBodyAndFinish(node, type, isMethod = false) {
    if (this.match(14)) {
      node.returnType = this.tsParseTypeOrTypePredicateAnnotation(14);
    }
    const bodilessType = type === "FunctionDeclaration" ? "TSDeclareFunction" : type === "ClassMethod" || type === "ClassPrivateMethod" ? "TSDeclareMethod" : undefined;
    if (bodilessType && !this.match(5) && this.isLineTerminator()) {
      return this.finishNode(node, bodilessType);
    }
    if (bodilessType === "TSDeclareFunction" && this.state.isAmbientContext) {
      this.raise(TSErrors.DeclareFunctionHasImplementation, {
        at: node
      });
      if (node.declare) {
        return super.parseFunctionBodyAndFinish(node, bodilessType, isMethod);
      }
    }
    this.tsDisallowOptionalPattern(node);
    return super.parseFunctionBodyAndFinish(node, type, isMethod);
  }
  registerFunctionStatementId(node) {
    if (!node.body && node.id) {
      this.checkIdentifier(node.id, _scopeflags.BIND_TS_AMBIENT);
    } else {
      super.registerFunctionStatementId(node);
    }
  }
  tsCheckForInvalidTypeCasts(items) {
    items.forEach(node => {
      if ((node == null ? void 0 : node.type) === "TSTypeCastExpression") {
        this.raise(TSErrors.UnexpectedTypeAnnotation, {
          at: node.typeAnnotation
        });
      }
    });
  }
  toReferencedList(exprList, isInParens) {
    this.tsCheckForInvalidTypeCasts(exprList);
    return exprList;
  }
  parseArrayLike(close, canBePattern, isTuple, refExpressionErrors) {
    const node = super.parseArrayLike(close, canBePattern, isTuple, refExpressionErrors);
    if (node.type === "ArrayExpression") {
      this.tsCheckForInvalidTypeCasts(node.elements);
    }
    return node;
  }
  parseSubscript(base, startLoc, noCalls, state) {
    if (!this.hasPrecedingLineBreak() && this.match(35)) {
      this.state.canStartJSXElement = false;
      this.next();
      const nonNullExpression = this.startNodeAt(startLoc);
      nonNullExpression.expression = base;
      return this.finishNode(nonNullExpression, "TSNonNullExpression");
    }
    let isOptionalCall = false;
    if (this.match(18) && this.lookaheadCharCode() === 60) {
      if (noCalls) {
        state.stop = true;
        return base;
      }
      state.optionalChainMember = isOptionalCall = true;
      this.next();
    }
    if (this.match(47) || this.match(51)) {
      let missingParenErrorLoc;
      const result = this.tsTryParseAndCatch(() => {
        if (!noCalls && this.atPossibleAsyncArrow(base)) {
          const asyncArrowFn = this.tsTryParseGenericAsyncArrowFunction(startLoc);
          if (asyncArrowFn) {
            return asyncArrowFn;
          }
        }
        const typeArguments = this.tsParseTypeArgumentsInExpression();
        if (!typeArguments) return;
        if (isOptionalCall && !this.match(10)) {
          missingParenErrorLoc = this.state.curPosition();
          return;
        }
        if ((0, _types.tokenIsTemplate)(this.state.type)) {
          const result = super.parseTaggedTemplateExpression(base, startLoc, state);
          result.typeParameters = typeArguments;
          return result;
        }
        if (!noCalls && this.eat(10)) {
          const node = this.startNodeAt(startLoc);
          node.callee = base;
          node.arguments = this.parseCallExpressionArguments(11, false);
          this.tsCheckForInvalidTypeCasts(node.arguments);
          node.typeParameters = typeArguments;
          if (state.optionalChainMember) {
            node.optional = isOptionalCall;
          }
          return this.finishCallExpression(node, state.optionalChainMember);
        }
        const tokenType = this.state.type;
        if (tokenType === 48 || tokenType === 52 || tokenType !== 10 && (0, _types.tokenCanStartExpression)(tokenType) && !this.hasPrecedingLineBreak()) {
          return;
        }
        const node = this.startNodeAt(startLoc);
        node.expression = base;
        node.typeParameters = typeArguments;
        return this.finishNode(node, "TSInstantiationExpression");
      });
      if (missingParenErrorLoc) {
        this.unexpected(missingParenErrorLoc, 10);
      }
      if (result) {
        if (result.type === "TSInstantiationExpression" && (this.match(16) || this.match(18) && this.lookaheadCharCode() !== 40)) {
          this.raise(TSErrors.InvalidPropertyAccessAfterInstantiationExpression, {
            at: this.state.startLoc
          });
        }
        return result;
      }
    }
    return super.parseSubscript(base, startLoc, noCalls, state);
  }
  parseNewCallee(node) {
    var _callee$extra;
    super.parseNewCallee(node);
    const {
      callee
    } = node;
    if (callee.type === "TSInstantiationExpression" && !((_callee$extra = callee.extra) != null && _callee$extra.parenthesized)) {
      node.typeParameters = callee.typeParameters;
      node.callee = callee.expression;
    }
  }
  parseExprOp(left, leftStartLoc, minPrec) {
    let isSatisfies;
    if ((0, _types.tokenOperatorPrecedence)(58) > minPrec && !this.hasPrecedingLineBreak() && (this.isContextual(93) || (isSatisfies = this.isContextual(118)))) {
      const node = this.startNodeAt(leftStartLoc);
      node.expression = left;
      node.typeAnnotation = this.tsInType(() => {
        this.next();
        if (this.match(75)) {
          if (isSatisfies) {
            this.raise(_parseError.Errors.UnexpectedKeyword, {
              at: this.state.startLoc,
              keyword: "const"
            });
          }
          return this.tsParseTypeReference();
        }
        return this.tsParseType();
      });
      this.finishNode(node, isSatisfies ? "TSSatisfiesExpression" : "TSAsExpression");
      this.reScan_lt_gt();
      return this.parseExprOp(node, leftStartLoc, minPrec);
    }
    return super.parseExprOp(left, leftStartLoc, minPrec);
  }
  checkReservedWord(word, startLoc, checkKeywords, isBinding) {
    if (!this.state.isAmbientContext) {
      super.checkReservedWord(word, startLoc, checkKeywords, isBinding);
    }
  }
  checkImportReflection(node) {
    super.checkImportReflection(node);
    if (node.module && node.importKind !== "value") {
      this.raise(TSErrors.ImportReflectionHasImportType, {
        at: node.specifiers[0].loc.start
      });
    }
  }
  checkDuplicateExports() {}
  isPotentialImportPhase(isExport) {
    if (super.isPotentialImportPhase(isExport)) return true;
    if (this.isContextual(128)) {
      const ch = this.lookaheadCharCode();
      return isExport ? ch === 123 || ch === 42 : ch !== 61;
    }
    return !isExport && this.isContextual(87);
  }
  applyImportPhase(node, isExport, phase, loc) {
    super.applyImportPhase(node, isExport, phase, loc);
    if (isExport) {
      node.exportKind = phase === "type" ? "type" : "value";
    } else {
      node.importKind = phase === "type" || phase === "typeof" ? phase : "value";
    }
  }
  parseImport(node) {
    if (this.match(131)) {
      node.importKind = "value";
      return super.parseImport(node);
    }
    let importNode;
    if ((0, _types.tokenIsIdentifier)(this.state.type) && this.lookaheadCharCode() === 61) {
      node.importKind = "value";
      return this.tsParseImportEqualsDeclaration(node);
    } else if (this.isContextual(128)) {
      const maybeDefaultIdentifier = this.parseMaybeImportPhase(node, false);
      if (this.lookaheadCharCode() === 61) {
        return this.tsParseImportEqualsDeclaration(node, maybeDefaultIdentifier);
      } else {
        importNode = super.parseImportSpecifiersAndAfter(node, maybeDefaultIdentifier);
      }
    } else {
      importNode = super.parseImport(node);
    }
    if (importNode.importKind === "type" && importNode.specifiers.length > 1 && importNode.specifiers[0].type === "ImportDefaultSpecifier") {
      this.raise(TSErrors.TypeImportCannotSpecifyDefaultAndNamed, {
        at: importNode
      });
    }
    return importNode;
  }
  parseExport(node, decorators) {
    if (this.match(83)) {
      this.next();
      let maybeDefaultIdentifier = null;
      if (this.isContextual(128) && this.isPotentialImportPhase(false)) {
        maybeDefaultIdentifier = this.parseMaybeImportPhase(node, false);
      } else {
        node.importKind = "value";
      }
      return this.tsParseImportEqualsDeclaration(node, maybeDefaultIdentifier, true);
    } else if (this.eat(29)) {
      const assign = node;
      assign.expression = super.parseExpression();
      this.semicolon();
      this.sawUnambiguousESM = true;
      return this.finishNode(assign, "TSExportAssignment");
    } else if (this.eatContextual(93)) {
      const decl = node;
      this.expectContextual(126);
      decl.id = this.parseIdentifier();
      this.semicolon();
      return this.finishNode(decl, "TSNamespaceExportDeclaration");
    } else {
      return super.parseExport(node, decorators);
    }
  }
  isAbstractClass() {
    return this.isContextual(122) && this.lookahead().type === 80;
  }
  parseExportDefaultExpression() {
    if (this.isAbstractClass()) {
      const cls = this.startNode();
      this.next();
      cls.abstract = true;
      return this.parseClass(cls, true, true);
    }
    if (this.match(127)) {
      const result = this.tsParseInterfaceDeclaration(this.startNode());
      if (result) return result;
    }
    return super.parseExportDefaultExpression();
  }
  parseVarStatement(node, kind, allowMissingInitializer = false) {
    const {
      isAmbientContext
    } = this.state;
    const declaration = super.parseVarStatement(node, kind, allowMissingInitializer || isAmbientContext);
    if (!isAmbientContext) return declaration;
    for (const {
      id,
      init
    } of declaration.declarations) {
      if (!init) continue;
      if (kind !== "const" || !!id.typeAnnotation) {
        this.raise(TSErrors.InitializerNotAllowedInAmbientContext, {
          at: init
        });
      } else if (!isValidAmbientConstInitializer(init, this.hasPlugin("estree"))) {
        this.raise(TSErrors.ConstInitiailizerMustBeStringOrNumericLiteralOrLiteralEnumReference, {
          at: init
        });
      }
    }
    return declaration;
  }
  parseStatementContent(flags, decorators) {
    if (this.match(75) && this.isLookaheadContextual("enum")) {
      const node = this.startNode();
      this.expect(75);
      return this.tsParseEnumDeclaration(node, {
        const: true
      });
    }
    if (this.isContextual(124)) {
      return this.tsParseEnumDeclaration(this.startNode());
    }
    if (this.isContextual(127)) {
      const result = this.tsParseInterfaceDeclaration(this.startNode());
      if (result) return result;
    }
    return super.parseStatementContent(flags, decorators);
  }
  parseAccessModifier() {
    return this.tsParseModifier(["public", "protected", "private"]);
  }
  tsHasSomeModifiers(member, modifiers) {
    return modifiers.some(modifier => {
      if (tsIsAccessModifier(modifier)) {
        return member.accessibility === modifier;
      }
      return !!member[modifier];
    });
  }
  tsIsStartOfStaticBlocks() {
    return this.isContextual(104) && this.lookaheadCharCode() === 123;
  }
  parseClassMember(classBody, member, state) {
    const modifiers = ["declare", "private", "public", "protected", "override", "abstract", "readonly", "static"];
    this.tsParseModifiers({
      allowedModifiers: modifiers,
      disallowedModifiers: ["in", "out"],
      stopOnStartOfClassStaticBlock: true,
      errorTemplate: TSErrors.InvalidModifierOnTypeParameterPositions
    }, member);
    const callParseClassMemberWithIsStatic = () => {
      if (this.tsIsStartOfStaticBlocks()) {
        this.next();
        this.next();
        if (this.tsHasSomeModifiers(member, modifiers)) {
          this.raise(TSErrors.StaticBlockCannotHaveModifier, {
            at: this.state.curPosition()
          });
        }
        super.parseClassStaticBlock(classBody, member);
      } else {
        this.parseClassMemberWithIsStatic(classBody, member, state, !!member.static);
      }
    };
    if (member.declare) {
      this.tsInAmbientContext(callParseClassMemberWithIsStatic);
    } else {
      callParseClassMemberWithIsStatic();
    }
  }
  parseClassMemberWithIsStatic(classBody, member, state, isStatic) {
    const idx = this.tsTryParseIndexSignature(member);
    if (idx) {
      classBody.body.push(idx);
      if (member.abstract) {
        this.raise(TSErrors.IndexSignatureHasAbstract, {
          at: member
        });
      }
      if (member.accessibility) {
        this.raise(TSErrors.IndexSignatureHasAccessibility, {
          at: member,
          modifier: member.accessibility
        });
      }
      if (member.declare) {
        this.raise(TSErrors.IndexSignatureHasDeclare, {
          at: member
        });
      }
      if (member.override) {
        this.raise(TSErrors.IndexSignatureHasOverride, {
          at: member
        });
      }
      return;
    }
    if (!this.state.inAbstractClass && member.abstract) {
      this.raise(TSErrors.NonAbstractClassHasAbstractMethod, {
        at: member
      });
    }
    if (member.override) {
      if (!state.hadSuperClass) {
        this.raise(TSErrors.OverrideNotInSubClass, {
          at: member
        });
      }
    }
    super.parseClassMemberWithIsStatic(classBody, member, state, isStatic);
  }
  parsePostMemberNameModifiers(methodOrProp) {
    const optional = this.eat(17);
    if (optional) methodOrProp.optional = true;
    if (methodOrProp.readonly && this.match(10)) {
      this.raise(TSErrors.ClassMethodHasReadonly, {
        at: methodOrProp
      });
    }
    if (methodOrProp.declare && this.match(10)) {
      this.raise(TSErrors.ClassMethodHasDeclare, {
        at: methodOrProp
      });
    }
  }
  parseExpressionStatement(node, expr, decorators) {
    const decl = expr.type === "Identifier" ? this.tsParseExpressionStatement(node, expr, decorators) : undefined;
    return decl || super.parseExpressionStatement(node, expr, decorators);
  }
  shouldParseExportDeclaration() {
    if (this.tsIsDeclarationStart()) return true;
    return super.shouldParseExportDeclaration();
  }
  parseConditional(expr, startLoc, refExpressionErrors) {
    if (!this.state.maybeInArrowParameters || !this.match(17)) {
      return super.parseConditional(expr, startLoc, refExpressionErrors);
    }
    const result = this.tryParse(() => super.parseConditional(expr, startLoc));
    if (!result.node) {
      if (result.error) {
        super.setOptionalParametersError(refExpressionErrors, result.error);
      }
      return expr;
    }
    if (result.error) this.state = result.failState;
    return result.node;
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
      typeCastNode.typeAnnotation = this.tsParseTypeAnnotation();
      return this.finishNode(typeCastNode, "TSTypeCastExpression");
    }
    return node;
  }
  parseExportDeclaration(node) {
    if (!this.state.isAmbientContext && this.isContextual(123)) {
      return this.tsInAmbientContext(() => this.parseExportDeclaration(node));
    }
    const startLoc = this.state.startLoc;
    const isDeclare = this.eatContextual(123);
    if (isDeclare && (this.isContextual(123) || !this.shouldParseExportDeclaration())) {
      throw this.raise(TSErrors.ExpectedAmbientAfterExportDeclare, {
        at: this.state.startLoc
      });
    }
    const isIdentifier = (0, _types.tokenIsIdentifier)(this.state.type);
    const declaration = isIdentifier && this.tsTryParseExportDeclaration() || super.parseExportDeclaration(node);
    if (!declaration) return null;
    if (declaration.type === "TSInterfaceDeclaration" || declaration.type === "TSTypeAliasDeclaration" || isDeclare) {
      node.exportKind = "type";
    }
    if (isDeclare) {
      this.resetStartLocation(declaration, startLoc);
      declaration.declare = true;
    }
    return declaration;
  }
  parseClassId(node, isStatement, optionalId, bindingType) {
    if ((!isStatement || optionalId) && this.isContextual(111)) {
      return;
    }
    super.parseClassId(node, isStatement, optionalId, node.declare ? _scopeflags.BIND_TS_AMBIENT : _scopeflags.BIND_CLASS);
    const typeParameters = this.tsTryParseTypeParameters(this.tsParseInOutConstModifiers);
    if (typeParameters) node.typeParameters = typeParameters;
  }
  parseClassPropertyAnnotation(node) {
    if (!node.optional) {
      if (this.eat(35)) {
        node.definite = true;
      } else if (this.eat(17)) {
        node.optional = true;
      }
    }
    const type = this.tsTryParseTypeAnnotation();
    if (type) node.typeAnnotation = type;
  }
  parseClassProperty(node) {
    this.parseClassPropertyAnnotation(node);
    if (this.state.isAmbientContext && !(node.readonly && !node.typeAnnotation) && this.match(29)) {
      this.raise(TSErrors.DeclareClassFieldHasInitializer, {
        at: this.state.startLoc
      });
    }
    if (node.abstract && this.match(29)) {
      const {
        key
      } = node;
      this.raise(TSErrors.AbstractPropertyHasInitializer, {
        at: this.state.startLoc,
        propertyName: key.type === "Identifier" && !node.computed ? key.name : `[${this.input.slice(key.start, key.end)}]`
      });
    }
    return super.parseClassProperty(node);
  }
  parseClassPrivateProperty(node) {
    if (node.abstract) {
      this.raise(TSErrors.PrivateElementHasAbstract, {
        at: node
      });
    }
    if (node.accessibility) {
      this.raise(TSErrors.PrivateElementHasAccessibility, {
        at: node,
        modifier: node.accessibility
      });
    }
    this.parseClassPropertyAnnotation(node);
    return super.parseClassPrivateProperty(node);
  }
  parseClassAccessorProperty(node) {
    this.parseClassPropertyAnnotation(node);
    if (node.optional) {
      this.raise(TSErrors.AccessorCannotBeOptional, {
        at: node
      });
    }
    return super.parseClassAccessorProperty(node);
  }
  pushClassMethod(classBody, method, isGenerator, isAsync, isConstructor, allowsDirectSuper) {
    const typeParameters = this.tsTryParseTypeParameters(this.tsParseConstModifier);
    if (typeParameters && isConstructor) {
      this.raise(TSErrors.ConstructorHasTypeParameters, {
        at: typeParameters
      });
    }
    const {
      declare = false,
      kind
    } = method;
    if (declare && (kind === "get" || kind === "set")) {
      this.raise(TSErrors.DeclareAccessor, {
        at: method,
        kind
      });
    }
    if (typeParameters) method.typeParameters = typeParameters;
    super.pushClassMethod(classBody, method, isGenerator, isAsync, isConstructor, allowsDirectSuper);
  }
  pushClassPrivateMethod(classBody, method, isGenerator, isAsync) {
    const typeParameters = this.tsTryParseTypeParameters(this.tsParseConstModifier);
    if (typeParameters) method.typeParameters = typeParameters;
    super.pushClassPrivateMethod(classBody, method, isGenerator, isAsync);
  }
  declareClassPrivateMethodInScope(node, kind) {
    if (node.type === "TSDeclareMethod") return;
    if (node.type === "MethodDefinition" && !node.value.body) return;
    super.declareClassPrivateMethodInScope(node, kind);
  }
  parseClassSuper(node) {
    super.parseClassSuper(node);
    if (node.superClass && (this.match(47) || this.match(51))) {
      node.superTypeParameters = this.tsParseTypeArgumentsInExpression();
    }
    if (this.eatContextual(111)) {
      node.implements = this.tsParseHeritageClause("implements");
    }
  }
  parseObjPropValue(prop, startLoc, isGenerator, isAsync, isPattern, isAccessor, refExpressionErrors) {
    const typeParameters = this.tsTryParseTypeParameters(this.tsParseConstModifier);
    if (typeParameters) prop.typeParameters = typeParameters;
    return super.parseObjPropValue(prop, startLoc, isGenerator, isAsync, isPattern, isAccessor, refExpressionErrors);
  }
  parseFunctionParams(node, isConstructor) {
    const typeParameters = this.tsTryParseTypeParameters(this.tsParseConstModifier);
    if (typeParameters) node.typeParameters = typeParameters;
    super.parseFunctionParams(node, isConstructor);
  }
  parseVarId(decl, kind) {
    super.parseVarId(decl, kind);
    if (decl.id.type === "Identifier" && !this.hasPrecedingLineBreak() && this.eat(35)) {
      decl.definite = true;
    }
    const type = this.tsTryParseTypeAnnotation();
    if (type) {
      decl.id.typeAnnotation = type;
      this.resetEndLocation(decl.id);
    }
  }
  parseAsyncArrowFromCallExpression(node, call) {
    if (this.match(14)) {
      node.returnType = this.tsParseTypeAnnotation();
    }
    return super.parseAsyncArrowFromCallExpression(node, call);
  }
  parseMaybeAssign(refExpressionErrors, afterLeftParse) {
    var _jsx, _jsx2, _typeCast, _jsx3, _typeCast2;
    let state;
    let jsx;
    let typeCast;
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
    if (!((_jsx = jsx) != null && _jsx.error) && !this.match(47)) {
      return super.parseMaybeAssign(refExpressionErrors, afterLeftParse);
    }
    if (!state || state === this.state) state = this.state.clone();
    let typeParameters;
    const arrow = this.tryParse(abort => {
      var _expr$extra, _typeParameters;
      typeParameters = this.tsParseTypeParameters(this.tsParseConstModifier);
      const expr = super.parseMaybeAssign(refExpressionErrors, afterLeftParse);
      if (expr.type !== "ArrowFunctionExpression" || (_expr$extra = expr.extra) != null && _expr$extra.parenthesized) {
        abort();
      }
      if (((_typeParameters = typeParameters) == null ? void 0 : _typeParameters.params.length) !== 0) {
        this.resetStartLocationFromNode(expr, typeParameters);
      }
      expr.typeParameters = typeParameters;
      ;
      return expr;
    }, state);
    if (!arrow.error && !arrow.aborted) {
      if (typeParameters) this.reportReservedArrowTypeParam(typeParameters);
      return arrow.node;
    }
    if (!jsx) {
      assert(!this.hasPlugin("jsx"));
      typeCast = this.tryParse(() => super.parseMaybeAssign(refExpressionErrors, afterLeftParse), state);
      if (!typeCast.error) return typeCast.node;
    }
    if ((_jsx2 = jsx) != null && _jsx2.node) {
      this.state = jsx.failState;
      return jsx.node;
    }
    if (arrow.node) {
      this.state = arrow.failState;
      if (typeParameters) this.reportReservedArrowTypeParam(typeParameters);
      return arrow.node;
    }
    if ((_typeCast = typeCast) != null && _typeCast.node) {
      this.state = typeCast.failState;
      return typeCast.node;
    }
    throw ((_jsx3 = jsx) == null ? void 0 : _jsx3.error) || arrow.error || ((_typeCast2 = typeCast) == null ? void 0 : _typeCast2.error);
  }
  reportReservedArrowTypeParam(node) {
    var _node$extra;
    if (node.params.length === 1 && !node.params[0].constraint && !((_node$extra = node.extra) != null && _node$extra.trailingComma) && this.getPluginOption("typescript", "disallowAmbiguousJSXLike")) {
      this.raise(TSErrors.ReservedArrowTypeParam, {
        at: node
      });
    }
  }
  parseMaybeUnary(refExpressionErrors, sawUnary) {
    if (!this.hasPlugin("jsx") && this.match(47)) {
      return this.tsParseTypeAssertion();
    }
    return super.parseMaybeUnary(refExpressionErrors, sawUnary);
  }
  parseArrow(node) {
    if (this.match(14)) {
      const result = this.tryParse(abort => {
        const returnType = this.tsParseTypeOrTypePredicateAnnotation(14);
        if (this.canInsertSemicolon() || !this.match(19)) abort();
        return returnType;
      });
      if (result.aborted) return;
      if (!result.thrown) {
        if (result.error) this.state = result.failState;
        node.returnType = result.node;
      }
    }
    return super.parseArrow(node);
  }
  parseAssignableListItemTypes(param, flags) {
    if (!(flags & _lval.ParseBindingListFlags.IS_FUNCTION_PARAMS)) return param;
    if (this.eat(17)) {
      param.optional = true;
    }
    const type = this.tsTryParseTypeAnnotation();
    if (type) param.typeAnnotation = type;
    this.resetEndLocation(param);
    return param;
  }
  isAssignable(node, isBinding) {
    switch (node.type) {
      case "TSTypeCastExpression":
        return this.isAssignable(node.expression, isBinding);
      case "TSParameterProperty":
        return true;
      default:
        return super.isAssignable(node, isBinding);
    }
  }
  toAssignable(node, isLHS = false) {
    switch (node.type) {
      case "ParenthesizedExpression":
        this.toAssignableParenthesizedExpression(node, isLHS);
        break;
      case "TSAsExpression":
      case "TSSatisfiesExpression":
      case "TSNonNullExpression":
      case "TSTypeAssertion":
        if (isLHS) {
          this.expressionScope.recordArrowParameterBindingError(TSErrors.UnexpectedTypeCastInParameter, {
            at: node
          });
        } else {
          this.raise(TSErrors.UnexpectedTypeCastInParameter, {
            at: node
          });
        }
        this.toAssignable(node.expression, isLHS);
        break;
      case "AssignmentExpression":
        if (!isLHS && node.left.type === "TSTypeCastExpression") {
          node.left = this.typeCastToParameter(node.left);
        }
      default:
        super.toAssignable(node, isLHS);
    }
  }
  toAssignableParenthesizedExpression(node, isLHS) {
    switch (node.expression.type) {
      case "TSAsExpression":
      case "TSSatisfiesExpression":
      case "TSNonNullExpression":
      case "TSTypeAssertion":
      case "ParenthesizedExpression":
        this.toAssignable(node.expression, isLHS);
        break;
      default:
        super.toAssignable(node, isLHS);
    }
  }
  checkToRestConversion(node, allowPattern) {
    switch (node.type) {
      case "TSAsExpression":
      case "TSSatisfiesExpression":
      case "TSTypeAssertion":
      case "TSNonNullExpression":
        this.checkToRestConversion(node.expression, false);
        break;
      default:
        super.checkToRestConversion(node, allowPattern);
    }
  }
  isValidLVal(type, isUnparenthesizedInAssign, binding) {
    return getOwn({
      TSTypeCastExpression: true,
      TSParameterProperty: "parameter",
      TSNonNullExpression: "expression",
      TSAsExpression: (binding !== _scopeflags.BIND_NONE || !isUnparenthesizedInAssign) && ["expression", true],
      TSSatisfiesExpression: (binding !== _scopeflags.BIND_NONE || !isUnparenthesizedInAssign) && ["expression", true],
      TSTypeAssertion: (binding !== _scopeflags.BIND_NONE || !isUnparenthesizedInAssign) && ["expression", true]
    }, type) || super.isValidLVal(type, isUnparenthesizedInAssign, binding);
  }
  parseBindingAtom() {
    if (this.state.type === 78) {
      return this.parseIdentifier(true);
    }
    return super.parseBindingAtom();
  }
  parseMaybeDecoratorArguments(expr) {
    if (this.match(47) || this.match(51)) {
      const typeArguments = this.tsParseTypeArgumentsInExpression();
      if (this.match(10)) {
        const call = super.parseMaybeDecoratorArguments(expr);
        call.typeParameters = typeArguments;
        return call;
      }
      this.unexpected(null, 10);
    }
    return super.parseMaybeDecoratorArguments(expr);
  }
  checkCommaAfterRest(close) {
    if (this.state.isAmbientContext && this.match(12) && this.lookaheadCharCode() === close) {
      this.next();
      return false;
    }
    return super.checkCommaAfterRest(close);
  }
  isClassMethod() {
    return this.match(47) || super.isClassMethod();
  }
  isClassProperty() {
    return this.match(35) || this.match(14) || super.isClassProperty();
  }
  parseMaybeDefault(startLoc, left) {
    const node = super.parseMaybeDefault(startLoc, left);
    if (node.type === "AssignmentPattern" && node.typeAnnotation && node.right.start < node.typeAnnotation.start) {
      this.raise(TSErrors.TypeAnnotationAfterAssign, {
        at: node.typeAnnotation
      });
    }
    return node;
  }
  getTokenFromCode(code) {
    if (this.state.inType) {
      if (code === 62) {
        this.finishOp(48, 1);
        return;
      }
      if (code === 60) {
        this.finishOp(47, 1);
        return;
      }
    }
    super.getTokenFromCode(code);
  }
  reScan_lt_gt() {
    const {
      type
    } = this.state;
    if (type === 47) {
      this.state.pos -= 1;
      this.readToken_lt();
    } else if (type === 48) {
      this.state.pos -= 1;
      this.readToken_gt();
    }
  }
  reScan_lt() {
    const {
      type
    } = this.state;
    if (type === 51) {
      this.state.pos -= 2;
      this.finishOp(47, 1);
      return 47;
    }
    return type;
  }
  toAssignableList(exprList, trailingCommaLoc, isLHS) {
    for (let i = 0; i < exprList.length; i++) {
      const expr = exprList[i];
      if ((expr == null ? void 0 : expr.type) === "TSTypeCastExpression") {
        exprList[i] = this.typeCastToParameter(expr);
      }
    }
    super.toAssignableList(exprList, trailingCommaLoc, isLHS);
  }
  typeCastToParameter(node) {
    node.expression.typeAnnotation = node.typeAnnotation;
    this.resetEndLocation(node.expression, node.typeAnnotation.loc.end);
    return node.expression;
  }
  shouldParseArrow(params) {
    if (this.match(14)) {
      return params.every(expr => this.isAssignable(expr, true));
    }
    return super.shouldParseArrow(params);
  }
  shouldParseAsyncArrow() {
    return this.match(14) || super.shouldParseAsyncArrow();
  }
  canHaveLeadingDecorator() {
    return super.canHaveLeadingDecorator() || this.isAbstractClass();
  }
  jsxParseOpeningElementAfterName(node) {
    if (this.match(47) || this.match(51)) {
      const typeArguments = this.tsTryParseAndCatch(() => this.tsParseTypeArgumentsInExpression());
      if (typeArguments) node.typeParameters = typeArguments;
    }
    return super.jsxParseOpeningElementAfterName(node);
  }
  getGetterSetterExpectedParamCount(method) {
    const baseCount = super.getGetterSetterExpectedParamCount(method);
    const params = this.getObjectOrClassMethodParams(method);
    const firstParam = params[0];
    const hasContextParam = firstParam && this.isThisParam(firstParam);
    return hasContextParam ? baseCount + 1 : baseCount;
  }
  parseCatchClauseParam() {
    const param = super.parseCatchClauseParam();
    const type = this.tsTryParseTypeAnnotation();
    if (type) {
      param.typeAnnotation = type;
      this.resetEndLocation(param);
    }
    return param;
  }
  tsInAmbientContext(cb) {
    const oldIsAmbientContext = this.state.isAmbientContext;
    this.state.isAmbientContext = true;
    try {
      return cb();
    } finally {
      this.state.isAmbientContext = oldIsAmbientContext;
    }
  }
  parseClass(node, isStatement, optionalId) {
    const oldInAbstractClass = this.state.inAbstractClass;
    this.state.inAbstractClass = !!node.abstract;
    try {
      return super.parseClass(node, isStatement, optionalId);
    } finally {
      this.state.inAbstractClass = oldInAbstractClass;
    }
  }
  tsParseAbstractDeclaration(node, decorators) {
    if (this.match(80)) {
      node.abstract = true;
      return this.maybeTakeDecorators(decorators, this.parseClass(node, true, false));
    } else if (this.isContextual(127)) {
      if (!this.hasFollowingLineBreak()) {
        node.abstract = true;
        this.raise(TSErrors.NonClassMethodPropertyHasAbstractModifer, {
          at: node
        });
        return this.tsParseInterfaceDeclaration(node);
      }
    } else {
      this.unexpected(null, 80);
    }
  }
  parseMethod(node, isGenerator, isAsync, isConstructor, allowDirectSuper, type, inClassScope) {
    const method = super.parseMethod(node, isGenerator, isAsync, isConstructor, allowDirectSuper, type, inClassScope);
    if (method.abstract) {
      const hasBody = this.hasPlugin("estree") ? !!method.value.body : !!method.body;
      if (hasBody) {
        const {
          key
        } = method;
        this.raise(TSErrors.AbstractMethodHasImplementation, {
          at: method,
          methodName: key.type === "Identifier" && !method.computed ? key.name : `[${this.input.slice(key.start, key.end)}]`
        });
      }
    }
    return method;
  }
  tsParseTypeParameterName() {
    const typeName = this.parseIdentifier();
    return typeName.name;
  }
  shouldParseAsAmbientContext() {
    return !!this.getPluginOption("typescript", "dts");
  }
  parse() {
    if (this.shouldParseAsAmbientContext()) {
      this.state.isAmbientContext = true;
    }
    return super.parse();
  }
  getExpression() {
    if (this.shouldParseAsAmbientContext()) {
      this.state.isAmbientContext = true;
    }
    return super.getExpression();
  }
  parseExportSpecifier(node, isString, isInTypeExport, isMaybeTypeOnly) {
    if (!isString && isMaybeTypeOnly) {
      this.parseTypeOnlyImportExportSpecifier(node, false, isInTypeExport);
      return this.finishNode(node, "ExportSpecifier");
    }
    node.exportKind = "value";
    return super.parseExportSpecifier(node, isString, isInTypeExport, isMaybeTypeOnly);
  }
  parseImportSpecifier(specifier, importedIsString, isInTypeOnlyImport, isMaybeTypeOnly, bindingType) {
    if (!importedIsString && isMaybeTypeOnly) {
      this.parseTypeOnlyImportExportSpecifier(specifier, true, isInTypeOnlyImport);
      return this.finishNode(specifier, "ImportSpecifier");
    }
    specifier.importKind = "value";
    return super.parseImportSpecifier(specifier, importedIsString, isInTypeOnlyImport, isMaybeTypeOnly, isInTypeOnlyImport ? _scopeflags.BIND_TS_TYPE_IMPORT : _scopeflags.BIND_FLAGS_TS_IMPORT);
  }
  parseTypeOnlyImportExportSpecifier(node, isImport, isInTypeOnlyImportExport) {
    const leftOfAsKey = isImport ? "imported" : "local";
    const rightOfAsKey = isImport ? "local" : "exported";
    let leftOfAs = node[leftOfAsKey];
    let rightOfAs;
    let hasTypeSpecifier = false;
    let canParseAsKeyword = true;
    const loc = leftOfAs.loc.start;
    if (this.isContextual(93)) {
      const firstAs = this.parseIdentifier();
      if (this.isContextual(93)) {
        const secondAs = this.parseIdentifier();
        if ((0, _types.tokenIsKeywordOrIdentifier)(this.state.type)) {
          hasTypeSpecifier = true;
          leftOfAs = firstAs;
          rightOfAs = isImport ? this.parseIdentifier() : this.parseModuleExportName();
          canParseAsKeyword = false;
        } else {
          rightOfAs = secondAs;
          canParseAsKeyword = false;
        }
      } else if ((0, _types.tokenIsKeywordOrIdentifier)(this.state.type)) {
        canParseAsKeyword = false;
        rightOfAs = isImport ? this.parseIdentifier() : this.parseModuleExportName();
      } else {
        hasTypeSpecifier = true;
        leftOfAs = firstAs;
      }
    } else if ((0, _types.tokenIsKeywordOrIdentifier)(this.state.type)) {
      hasTypeSpecifier = true;
      if (isImport) {
        leftOfAs = this.parseIdentifier(true);
        if (!this.isContextual(93)) {
          this.checkReservedWord(leftOfAs.name, leftOfAs.loc.start, true, true);
        }
      } else {
        leftOfAs = this.parseModuleExportName();
      }
    }
    if (hasTypeSpecifier && isInTypeOnlyImportExport) {
      this.raise(isImport ? TSErrors.TypeModifierIsUsedInTypeImports : TSErrors.TypeModifierIsUsedInTypeExports, {
        at: loc
      });
    }
    node[leftOfAsKey] = leftOfAs;
    node[rightOfAsKey] = rightOfAs;
    const kindKey = isImport ? "importKind" : "exportKind";
    node[kindKey] = hasTypeSpecifier ? "type" : "value";
    if (canParseAsKeyword && this.eatContextual(93)) {
      node[rightOfAsKey] = isImport ? this.parseIdentifier() : this.parseModuleExportName();
    }
    if (!node[rightOfAsKey]) {
      node[rightOfAsKey] = (0, _node.cloneIdentifier)(node[leftOfAsKey]);
    }
    if (isImport) {
      this.checkIdentifier(node[rightOfAsKey], hasTypeSpecifier ? _scopeflags.BIND_TS_TYPE_IMPORT : _scopeflags.BIND_FLAGS_TS_IMPORT);
    }
  }
};
exports.default = _default;
function isPossiblyLiteralEnum(expression) {
  if (expression.type !== "MemberExpression") return false;
  const {
    computed,
    property
  } = expression;
  if (computed && property.type !== "StringLiteral" && (property.type !== "TemplateLiteral" || property.expressions.length > 0)) {
    return false;
  }
  return isUncomputedMemberExpressionChain(expression.object);
}
function isValidAmbientConstInitializer(expression, estree) {
  var _expression$extra;
  const {
    type
  } = expression;
  if ((_expression$extra = expression.extra) != null && _expression$extra.parenthesized) {
    return false;
  }
  if (estree) {
    if (type === "Literal") {
      const {
        value
      } = expression;
      if (typeof value === "string" || typeof value === "boolean") {
        return true;
      }
    }
  } else {
    if (type === "StringLiteral" || type === "BooleanLiteral") {
      return true;
    }
  }
  if (isNumber(expression, estree) || isNegativeNumber(expression, estree)) {
    return true;
  }
  if (type === "TemplateLiteral" && expression.expressions.length === 0) {
    return true;
  }
  if (isPossiblyLiteralEnum(expression)) {
    return true;
  }
  return false;
}
function isNumber(expression, estree) {
  if (estree) {
    return expression.type === "Literal" && (typeof expression.value === "number" || "bigint" in expression);
  }
  return expression.type === "NumericLiteral" || expression.type === "BigIntLiteral";
}
function isNegativeNumber(expression, estree) {
  if (expression.type === "UnaryExpression") {
    const {
      operator,
      argument
    } = expression;
    if (operator === "-" && isNumber(argument, estree)) {
      return true;
    }
  }
  return false;
}
function isUncomputedMemberExpressionChain(expression) {
  if (expression.type === "Identifier") return true;
  if (expression.type !== "MemberExpression" || expression.computed) {
    return false;
  }
  return isUncomputedMemberExpressionChain(expression.object);
}

//# sourceMappingURL=index.js.map
