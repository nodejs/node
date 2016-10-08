"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

exports.default = function (instance) {
  // plain function return types: function name(): string {}
  instance.extend("parseFunctionBody", function (inner) {
    return function (node, allowExpression) {
      if (this.match(_types.types.colon) && !allowExpression) {
        // if allowExpression is true then we're parsing an arrow function and if
        // there's a return type then it's been handled elsewhere
        node.returnType = this.flowParseTypeAnnotation();
      }

      return inner.call(this, node, allowExpression);
    };
  });

  // interfaces
  instance.extend("parseStatement", function (inner) {
    return function (declaration, topLevel) {
      // strict mode handling of `interface` since it's a reserved word
      if (this.state.strict && this.match(_types.types.name) && this.state.value === "interface") {
        var node = this.startNode();
        this.next();
        return this.flowParseInterface(node);
      } else {
        return inner.call(this, declaration, topLevel);
      }
    };
  });

  // declares, interfaces and type aliases
  instance.extend("parseExpressionStatement", function (inner) {
    return function (node, expr) {
      if (expr.type === "Identifier") {
        if (expr.name === "declare") {
          if (this.match(_types.types._class) || this.match(_types.types.name) || this.match(_types.types._function) || this.match(_types.types._var)) {
            return this.flowParseDeclare(node);
          }
        } else if (this.match(_types.types.name)) {
          if (expr.name === "interface") {
            return this.flowParseInterface(node);
          } else if (expr.name === "type") {
            return this.flowParseTypeAlias(node);
          }
        }
      }

      return inner.call(this, node, expr);
    };
  });

  // export type
  instance.extend("shouldParseExportDeclaration", function (inner) {
    return function () {
      return this.isContextual("type") || this.isContextual("interface") || inner.call(this);
    };
  });

  instance.extend("parseConditional", function (inner) {
    return function (expr, noIn, startPos, startLoc, refNeedsArrowPos) {
      // only do the expensive clone if there is a question mark
      // and if we come from inside parens
      if (refNeedsArrowPos && this.match(_types.types.question)) {
        var state = this.state.clone();
        try {
          return inner.call(this, expr, noIn, startPos, startLoc);
        } catch (err) {
          if (err instanceof SyntaxError) {
            this.state = state;
            refNeedsArrowPos.start = err.pos || this.state.start;
            return expr;
          } else {
            throw err;
          }
        }
      }

      return inner.call(this, expr, noIn, startPos, startLoc);
    };
  });

  instance.extend("parseParenItem", function (inner) {
    return function (node, startLoc, startPos) {
      node = inner.call(this, node, startLoc, startPos);
      if (this.eat(_types.types.question)) {
        node.optional = true;
      }

      if (this.match(_types.types.colon)) {
        var typeCastNode = this.startNodeAt(startLoc, startPos);
        typeCastNode.expression = node;
        typeCastNode.typeAnnotation = this.flowParseTypeAnnotation();

        return this.finishNode(typeCastNode, "TypeCastExpression");
      }

      return node;
    };
  });

  instance.extend("parseExport", function (inner) {
    return function (node) {
      node = inner.call(this, node);
      if (node.type === "ExportNamedDeclaration") {
        node.exportKind = node.exportKind || "value";
      }
      return node;
    };
  });

  instance.extend("parseExportDeclaration", function (inner) {
    return function (node) {
      if (this.isContextual("type")) {
        node.exportKind = "type";

        var declarationNode = this.startNode();
        this.next();

        if (this.match(_types.types.braceL)) {
          // export type { foo, bar };
          node.specifiers = this.parseExportSpecifiers();
          this.parseExportFrom(node);
          return null;
        } else {
          // export type Foo = Bar;
          return this.flowParseTypeAlias(declarationNode);
        }
      } else if (this.isContextual("interface")) {
        node.exportKind = "type";
        var _declarationNode = this.startNode();
        this.next();
        return this.flowParseInterface(_declarationNode);
      } else {
        return inner.call(this, node);
      }
    };
  });

  instance.extend("parseClassId", function (inner) {
    return function (node) {
      inner.apply(this, arguments);
      if (this.isRelational("<")) {
        node.typeParameters = this.flowParseTypeParameterDeclaration();
      }
    };
  });

  // don't consider `void` to be a keyword as then it'll use the void token type
  // and set startExpr
  instance.extend("isKeyword", function (inner) {
    return function (name) {
      if (this.state.inType && name === "void") {
        return false;
      } else {
        return inner.call(this, name);
      }
    };
  });

  // ensure that inside flow types, we bypass the jsx parser plugin
  instance.extend("readToken", function (inner) {
    return function (code) {
      if (this.state.inType && (code === 62 || code === 60)) {
        return this.finishOp(_types.types.relational, 1);
      } else {
        return inner.call(this, code);
      }
    };
  });

  // don't lex any token as a jsx one inside a flow type
  instance.extend("jsx_readToken", function (inner) {
    return function () {
      if (!this.state.inType) return inner.call(this);
    };
  });

  function typeCastToParameter(node) {
    node.expression.typeAnnotation = node.typeAnnotation;
    return node.expression;
  }

  instance.extend("toAssignable", function (inner) {
    return function (node) {
      if (node.type === "TypeCastExpression") {
        return typeCastToParameter(node);
      } else {
        return inner.apply(this, arguments);
      }
    };
  });

  // turn type casts that we found in function parameter head into type annotated params
  instance.extend("toAssignableList", function (inner) {
    return function (exprList, isBinding) {
      for (var i = 0; i < exprList.length; i++) {
        var expr = exprList[i];
        if (expr && expr.type === "TypeCastExpression") {
          exprList[i] = typeCastToParameter(expr);
        }
      }
      return inner.call(this, exprList, isBinding);
    };
  });

  // this is a list of nodes, from something like a call expression, we need to filter the
  // type casts that we've found that are illegal in this context
  instance.extend("toReferencedList", function () {
    return function (exprList) {
      for (var i = 0; i < exprList.length; i++) {
        var expr = exprList[i];
        if (expr && expr._exprListItem && expr.type === "TypeCastExpression") {
          this.raise(expr.start, "Unexpected type cast");
        }
      }

      return exprList;
    };
  });

  // parse an item inside a expression list eg. `(NODE, NODE)` where NODE represents
  // the position where this function is cal;ed
  instance.extend("parseExprListItem", function (inner) {
    return function (allowEmpty, refShorthandDefaultPos) {
      var container = this.startNode();
      var node = inner.call(this, allowEmpty, refShorthandDefaultPos);
      if (this.match(_types.types.colon)) {
        container._exprListItem = true;
        container.expression = node;
        container.typeAnnotation = this.flowParseTypeAnnotation();
        return this.finishNode(container, "TypeCastExpression");
      } else {
        return node;
      }
    };
  });

  instance.extend("checkLVal", function (inner) {
    return function (node) {
      if (node.type !== "TypeCastExpression") {
        return inner.apply(this, arguments);
      }
    };
  });

  // parse class property type annotations
  instance.extend("parseClassProperty", function (inner) {
    return function (node) {
      if (this.match(_types.types.colon)) {
        node.typeAnnotation = this.flowParseTypeAnnotation();
      }
      return inner.call(this, node);
    };
  });

  // determine whether or not we're currently in the position where a class property would appear
  instance.extend("isClassProperty", function (inner) {
    return function () {
      return this.match(_types.types.colon) || inner.call(this);
    };
  });

  // parse type parameters for class methods
  instance.extend("parseClassMethod", function () {
    return function (classBody, method, isGenerator, isAsync) {
      if (this.isRelational("<")) {
        method.typeParameters = this.flowParseTypeParameterDeclaration();
      }
      this.parseMethod(method, isGenerator, isAsync);
      classBody.body.push(this.finishNode(method, "ClassMethod"));
    };
  });

  // parse a the super class type parameters and implements
  instance.extend("parseClassSuper", function (inner) {
    return function (node, isStatement) {
      inner.call(this, node, isStatement);
      if (node.superClass && this.isRelational("<")) {
        node.superTypeParameters = this.flowParseTypeParameterInstantiation();
      }
      if (this.isContextual("implements")) {
        this.next();
        var implemented = node.implements = [];
        do {
          var _node = this.startNode();
          _node.id = this.parseIdentifier();
          if (this.isRelational("<")) {
            _node.typeParameters = this.flowParseTypeParameterInstantiation();
          } else {
            _node.typeParameters = null;
          }
          implemented.push(this.finishNode(_node, "ClassImplements"));
        } while (this.eat(_types.types.comma));
      }
    };
  });

  // parse type parameters for object method shorthand
  instance.extend("parseObjPropValue", function (inner) {
    return function (prop) {
      var typeParameters = void 0;

      // method shorthand
      if (this.isRelational("<")) {
        typeParameters = this.flowParseTypeParameterDeclaration();
        if (!this.match(_types.types.parenL)) this.unexpected();
      }

      inner.apply(this, arguments);

      // add typeParameters if we found them
      if (typeParameters) {
        (prop.value || prop).typeParameters = typeParameters;
      }
    };
  });

  instance.extend("parseAssignableListItemTypes", function () {
    return function (param) {
      if (this.eat(_types.types.question)) {
        param.optional = true;
      }
      if (this.match(_types.types.colon)) {
        param.typeAnnotation = this.flowParseTypeAnnotation();
      }
      this.finishNode(param, param.type);
      return param;
    };
  });

  // parse typeof and type imports
  instance.extend("parseImportSpecifiers", function (inner) {
    return function (node) {
      node.importKind = "value";

      var kind = null;
      if (this.match(_types.types._typeof)) {
        kind = "typeof";
      } else if (this.isContextual("type")) {
        kind = "type";
      }
      if (kind) {
        var lh = this.lookahead();
        if (lh.type === _types.types.name && lh.value !== "from" || lh.type === _types.types.braceL || lh.type === _types.types.star) {
          this.next();
          node.importKind = kind;
        }
      }

      inner.call(this, node);
    };
  });

  // parse function type parameters - function foo<T>() {}
  instance.extend("parseFunctionParams", function (inner) {
    return function (node) {
      var oldInType = this.state.inType;
      this.state.inType = true;
      if (this.isRelational("<")) {
        node.typeParameters = this.flowParseTypeParameterDeclaration();
      }
      this.state.inType = oldInType;
      inner.call(this, node);
    };
  });

  // parse flow type annotations on variable declarator heads - let foo: string = bar
  instance.extend("parseVarHead", function (inner) {
    return function (decl) {
      inner.call(this, decl);
      if (this.match(_types.types.colon)) {
        decl.id.typeAnnotation = this.flowParseTypeAnnotation();
        this.finishNode(decl.id, decl.id.type);
      }
    };
  });

  // parse the return type of an async arrow function - let foo = (async (): number => {});
  instance.extend("parseAsyncArrowFromCallExpression", function (inner) {
    return function (node, call) {
      if (this.match(_types.types.colon)) {
        node.returnType = this.flowParseTypeAnnotation();
      }

      return inner.call(this, node, call);
    };
  });

  // todo description
  instance.extend("shouldParseAsyncArrow", function (inner) {
    return function () {
      return this.match(_types.types.colon) || inner.call(this);
    };
  });

  // We need to support type parameter declarations for arrow functions. This
  // is tricky. There are three situations we need to handle
  //
  // 1. This is either JSX or an arrow function. We'll try JSX first. If that
  //    fails, we'll try an arrow function. If that fails, we'll throw the JSX
  //    error.
  // 2. This is an arrow function. We'll parse the type parameter declaration,
  //    parse the rest, make sure the rest is an arrow function, and go from
  //    there
  // 3. This is neither. Just call the inner function
  instance.extend("parseMaybeAssign", function (inner) {
    return function () {
      var jsxError = null;

      for (var _len = arguments.length, args = Array(_len), _key = 0; _key < _len; _key++) {
        args[_key] = arguments[_key];
      }

      if (_types.types.jsxTagStart && this.match(_types.types.jsxTagStart)) {
        var state = this.state.clone();
        try {
          return inner.apply(this, args);
        } catch (err) {
          if (err instanceof SyntaxError) {
            this.state = state;
            jsxError = err;
          } else {
            throw err;
          }
        }
      }

      // Need to push something onto the context to stop
      // the JSX plugin from messing with the tokens
      this.state.context.push(_context.types.parenExpression);
      if (jsxError != null || this.isRelational("<")) {
        var arrowExpression = void 0;
        var typeParameters = void 0;
        try {
          var oldInType = this.state.inType;
          this.state.inType = true;
          typeParameters = this.flowParseTypeParameterDeclaration();
          this.state.inType = oldInType;

          arrowExpression = inner.apply(this, args);
          arrowExpression.typeParameters = typeParameters;
        } catch (err) {
          throw jsxError || err;
        }

        if (arrowExpression.type === "ArrowFunctionExpression") {
          return arrowExpression;
        } else if (jsxError != null) {
          throw jsxError;
        } else {
          this.raise(typeParameters.start, "Expected an arrow function after this type parameter declaration");
        }
      }
      this.state.context.pop();

      return inner.apply(this, args);
    };
  });

  // handle return types for arrow functions
  instance.extend("parseArrow", function (inner) {
    return function (node) {
      if (this.match(_types.types.colon)) {
        var state = this.state.clone();
        try {
          var returnType = this.flowParseTypeAnnotation();
          if (!this.match(_types.types.arrow)) this.unexpected();
          // assign after it is clear it is an arrow
          node.returnType = returnType;
        } catch (err) {
          if (err instanceof SyntaxError) {
            this.state = state;
          } else {
            throw err;
          }
        }
      }

      return inner.call(this, node);
    };
  });

  instance.extend("isClassMutatorStarter", function (inner) {
    return function () {
      if (this.isRelational("<")) {
        return true;
      } else {
        return inner.call(this);
      }
    };
  });
};

var _types = require("../tokenizer/types");

var _context = require("../tokenizer/context");

var _parser = require("../parser");

var _parser2 = _interopRequireDefault(_parser);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

var pp = _parser2.default.prototype; /* eslint indent: 0 */
/* eslint max-len: 0 */

pp.flowParseTypeInitialiser = function (tok, allowLeadingPipeOrAnd) {
  var oldInType = this.state.inType;
  this.state.inType = true;
  this.expect(tok || _types.types.colon);
  if (allowLeadingPipeOrAnd) {
    if (this.match(_types.types.bitwiseAND) || this.match(_types.types.bitwiseOR)) {
      this.next();
    }
  }
  var type = this.flowParseType();
  this.state.inType = oldInType;
  return type;
};

pp.flowParseDeclareClass = function (node) {
  this.next();
  this.flowParseInterfaceish(node, true);
  return this.finishNode(node, "DeclareClass");
};

pp.flowParseDeclareFunction = function (node) {
  this.next();

  var id = node.id = this.parseIdentifier();

  var typeNode = this.startNode();
  var typeContainer = this.startNode();

  if (this.isRelational("<")) {
    typeNode.typeParameters = this.flowParseTypeParameterDeclaration();
  } else {
    typeNode.typeParameters = null;
  }

  this.expect(_types.types.parenL);
  var tmp = this.flowParseFunctionTypeParams();
  typeNode.params = tmp.params;
  typeNode.rest = tmp.rest;
  this.expect(_types.types.parenR);
  typeNode.returnType = this.flowParseTypeInitialiser();

  typeContainer.typeAnnotation = this.finishNode(typeNode, "FunctionTypeAnnotation");
  id.typeAnnotation = this.finishNode(typeContainer, "TypeAnnotation");

  this.finishNode(id, id.type);

  this.semicolon();

  return this.finishNode(node, "DeclareFunction");
};

pp.flowParseDeclare = function (node) {
  if (this.match(_types.types._class)) {
    return this.flowParseDeclareClass(node);
  } else if (this.match(_types.types._function)) {
    return this.flowParseDeclareFunction(node);
  } else if (this.match(_types.types._var)) {
    return this.flowParseDeclareVariable(node);
  } else if (this.isContextual("module")) {
    return this.flowParseDeclareModule(node);
  } else if (this.isContextual("type")) {
    return this.flowParseDeclareTypeAlias(node);
  } else if (this.isContextual("interface")) {
    return this.flowParseDeclareInterface(node);
  } else {
    this.unexpected();
  }
};

pp.flowParseDeclareVariable = function (node) {
  this.next();
  node.id = this.flowParseTypeAnnotatableIdentifier();
  this.semicolon();
  return this.finishNode(node, "DeclareVariable");
};

pp.flowParseDeclareModule = function (node) {
  this.next();

  if (this.match(_types.types.string)) {
    node.id = this.parseExprAtom();
  } else {
    node.id = this.parseIdentifier();
  }

  var bodyNode = node.body = this.startNode();
  var body = bodyNode.body = [];
  this.expect(_types.types.braceL);
  while (!this.match(_types.types.braceR)) {
    var node2 = this.startNode();

    // todo: declare check
    this.next();

    body.push(this.flowParseDeclare(node2));
  }
  this.expect(_types.types.braceR);

  this.finishNode(bodyNode, "BlockStatement");
  return this.finishNode(node, "DeclareModule");
};

pp.flowParseDeclareTypeAlias = function (node) {
  this.next();
  this.flowParseTypeAlias(node);
  return this.finishNode(node, "DeclareTypeAlias");
};

pp.flowParseDeclareInterface = function (node) {
  this.next();
  this.flowParseInterfaceish(node);
  return this.finishNode(node, "DeclareInterface");
};

// Interfaces

pp.flowParseInterfaceish = function (node, allowStatic) {
  node.id = this.parseIdentifier();

  if (this.isRelational("<")) {
    node.typeParameters = this.flowParseTypeParameterDeclaration();
  } else {
    node.typeParameters = null;
  }

  node.extends = [];
  node.mixins = [];

  if (this.eat(_types.types._extends)) {
    do {
      node.extends.push(this.flowParseInterfaceExtends());
    } while (this.eat(_types.types.comma));
  }

  if (this.isContextual("mixins")) {
    this.next();
    do {
      node.mixins.push(this.flowParseInterfaceExtends());
    } while (this.eat(_types.types.comma));
  }

  node.body = this.flowParseObjectType(allowStatic);
};

pp.flowParseInterfaceExtends = function () {
  var node = this.startNode();

  node.id = this.parseIdentifier();
  if (this.isRelational("<")) {
    node.typeParameters = this.flowParseTypeParameterInstantiation();
  } else {
    node.typeParameters = null;
  }

  return this.finishNode(node, "InterfaceExtends");
};

pp.flowParseInterface = function (node) {
  this.flowParseInterfaceish(node, false);
  return this.finishNode(node, "InterfaceDeclaration");
};

// Type aliases

pp.flowParseTypeAlias = function (node) {
  node.id = this.parseIdentifier();

  if (this.isRelational("<")) {
    node.typeParameters = this.flowParseTypeParameterDeclaration();
  } else {
    node.typeParameters = null;
  }

  node.right = this.flowParseTypeInitialiser(_types.types.eq,
  /*allowLeadingPipeOrAnd*/true);
  this.semicolon();

  return this.finishNode(node, "TypeAlias");
};

// Type annotations

pp.flowParseTypeParameter = function () {
  var node = this.startNode();

  var variance = void 0;
  if (this.match(_types.types.plusMin)) {
    if (this.state.value === "+") {
      variance = "plus";
    } else if (this.state.value === "-") {
      variance = "minus";
    }
    this.eat(_types.types.plusMin);
  }

  var ident = this.flowParseTypeAnnotatableIdentifier(false, false);
  node.name = ident.name;
  node.variance = variance;
  node.bound = ident.typeAnnotation;

  if (this.match(_types.types.eq)) {
    this.eat(_types.types.eq);
    node.default = this.flowParseType();
  }

  return this.finishNode(node, "TypeParameter");
};

pp.flowParseTypeParameterDeclaration = function () {
  var node = this.startNode();
  node.params = [];

  if (this.isRelational("<") || this.match(_types.types.jsxTagStart)) {
    this.next();
  } else {
    this.unexpected();
  }

  do {
    node.params.push(this.flowParseTypeParameter());
    if (!this.isRelational(">")) {
      this.expect(_types.types.comma);
    }
  } while (!this.isRelational(">"));
  this.expectRelational(">");

  return this.finishNode(node, "TypeParameterDeclaration");
};

pp.flowParseTypeParameterInstantiation = function () {
  var node = this.startNode(),
      oldInType = this.state.inType;
  node.params = [];

  this.state.inType = true;

  this.expectRelational("<");
  while (!this.isRelational(">")) {
    node.params.push(this.flowParseType());
    if (!this.isRelational(">")) {
      this.expect(_types.types.comma);
    }
  }
  this.expectRelational(">");

  this.state.inType = oldInType;

  return this.finishNode(node, "TypeParameterInstantiation");
};

pp.flowParseObjectPropertyKey = function () {
  return this.match(_types.types.num) || this.match(_types.types.string) ? this.parseExprAtom() : this.parseIdentifier(true);
};

pp.flowParseObjectTypeIndexer = function (node, isStatic) {
  node.static = isStatic;

  this.expect(_types.types.bracketL);
  node.id = this.flowParseObjectPropertyKey();
  node.key = this.flowParseTypeInitialiser();
  this.expect(_types.types.bracketR);
  node.value = this.flowParseTypeInitialiser();

  this.flowObjectTypeSemicolon();
  return this.finishNode(node, "ObjectTypeIndexer");
};

pp.flowParseObjectTypeMethodish = function (node) {
  node.params = [];
  node.rest = null;
  node.typeParameters = null;

  if (this.isRelational("<")) {
    node.typeParameters = this.flowParseTypeParameterDeclaration();
  }

  this.expect(_types.types.parenL);
  while (this.match(_types.types.name)) {
    node.params.push(this.flowParseFunctionTypeParam());
    if (!this.match(_types.types.parenR)) {
      this.expect(_types.types.comma);
    }
  }

  if (this.eat(_types.types.ellipsis)) {
    node.rest = this.flowParseFunctionTypeParam();
  }
  this.expect(_types.types.parenR);
  node.returnType = this.flowParseTypeInitialiser();

  return this.finishNode(node, "FunctionTypeAnnotation");
};

pp.flowParseObjectTypeMethod = function (startPos, startLoc, isStatic, key) {
  var node = this.startNodeAt(startPos, startLoc);
  node.value = this.flowParseObjectTypeMethodish(this.startNodeAt(startPos, startLoc));
  node.static = isStatic;
  node.key = key;
  node.optional = false;
  this.flowObjectTypeSemicolon();
  return this.finishNode(node, "ObjectTypeProperty");
};

pp.flowParseObjectTypeCallProperty = function (node, isStatic) {
  var valueNode = this.startNode();
  node.static = isStatic;
  node.value = this.flowParseObjectTypeMethodish(valueNode);
  this.flowObjectTypeSemicolon();
  return this.finishNode(node, "ObjectTypeCallProperty");
};

pp.flowParseObjectType = function (allowStatic) {
  var nodeStart = this.startNode();
  var node = void 0;
  var propertyKey = void 0;
  var isStatic = void 0;

  nodeStart.callProperties = [];
  nodeStart.properties = [];
  nodeStart.indexers = [];

  this.expect(_types.types.braceL);

  while (!this.match(_types.types.braceR)) {
    var optional = false;
    var startPos = this.state.start,
        startLoc = this.state.startLoc;
    node = this.startNode();
    if (allowStatic && this.isContextual("static")) {
      this.next();
      isStatic = true;
    }

    if (this.match(_types.types.bracketL)) {
      nodeStart.indexers.push(this.flowParseObjectTypeIndexer(node, isStatic));
    } else if (this.match(_types.types.parenL) || this.isRelational("<")) {
      nodeStart.callProperties.push(this.flowParseObjectTypeCallProperty(node, allowStatic));
    } else {
      if (isStatic && this.match(_types.types.colon)) {
        propertyKey = this.parseIdentifier();
      } else {
        propertyKey = this.flowParseObjectPropertyKey();
      }
      if (this.isRelational("<") || this.match(_types.types.parenL)) {
        // This is a method property
        nodeStart.properties.push(this.flowParseObjectTypeMethod(startPos, startLoc, isStatic, propertyKey));
      } else {
        if (this.eat(_types.types.question)) {
          optional = true;
        }
        node.key = propertyKey;
        node.value = this.flowParseTypeInitialiser();
        node.optional = optional;
        node.static = isStatic;
        this.flowObjectTypeSemicolon();
        nodeStart.properties.push(this.finishNode(node, "ObjectTypeProperty"));
      }
    }
  }

  this.expect(_types.types.braceR);

  return this.finishNode(nodeStart, "ObjectTypeAnnotation");
};

pp.flowObjectTypeSemicolon = function () {
  if (!this.eat(_types.types.semi) && !this.eat(_types.types.comma) && !this.match(_types.types.braceR)) {
    this.unexpected();
  }
};

pp.flowParseGenericType = function (startPos, startLoc, id) {
  var node = this.startNodeAt(startPos, startLoc);

  node.typeParameters = null;
  node.id = id;

  while (this.eat(_types.types.dot)) {
    var node2 = this.startNodeAt(startPos, startLoc);
    node2.qualification = node.id;
    node2.id = this.parseIdentifier();
    node.id = this.finishNode(node2, "QualifiedTypeIdentifier");
  }

  if (this.isRelational("<")) {
    node.typeParameters = this.flowParseTypeParameterInstantiation();
  }

  return this.finishNode(node, "GenericTypeAnnotation");
};

pp.flowParseTypeofType = function () {
  var node = this.startNode();
  this.expect(_types.types._typeof);
  node.argument = this.flowParsePrimaryType();
  return this.finishNode(node, "TypeofTypeAnnotation");
};

pp.flowParseTupleType = function () {
  var node = this.startNode();
  node.types = [];
  this.expect(_types.types.bracketL);
  // We allow trailing commas
  while (this.state.pos < this.input.length && !this.match(_types.types.bracketR)) {
    node.types.push(this.flowParseType());
    if (this.match(_types.types.bracketR)) break;
    this.expect(_types.types.comma);
  }
  this.expect(_types.types.bracketR);
  return this.finishNode(node, "TupleTypeAnnotation");
};

pp.flowParseFunctionTypeParam = function () {
  var optional = false;
  var node = this.startNode();
  node.name = this.parseIdentifier();
  if (this.eat(_types.types.question)) {
    optional = true;
  }
  node.optional = optional;
  node.typeAnnotation = this.flowParseTypeInitialiser();
  return this.finishNode(node, "FunctionTypeParam");
};

pp.flowParseFunctionTypeParams = function () {
  var ret = { params: [], rest: null };
  while (this.match(_types.types.name)) {
    ret.params.push(this.flowParseFunctionTypeParam());
    if (!this.match(_types.types.parenR)) {
      this.expect(_types.types.comma);
    }
  }
  if (this.eat(_types.types.ellipsis)) {
    ret.rest = this.flowParseFunctionTypeParam();
  }
  return ret;
};

pp.flowIdentToTypeAnnotation = function (startPos, startLoc, node, id) {
  switch (id.name) {
    case "any":
      return this.finishNode(node, "AnyTypeAnnotation");

    case "void":
      return this.finishNode(node, "VoidTypeAnnotation");

    case "bool":
    case "boolean":
      return this.finishNode(node, "BooleanTypeAnnotation");

    case "mixed":
      return this.finishNode(node, "MixedTypeAnnotation");

    case "number":
      return this.finishNode(node, "NumberTypeAnnotation");

    case "string":
      return this.finishNode(node, "StringTypeAnnotation");

    default:
      return this.flowParseGenericType(startPos, startLoc, id);
  }
};

// The parsing of types roughly parallels the parsing of expressions, and
// primary types are kind of like primary expressions...they're the
// primitives with which other types are constructed.
pp.flowParsePrimaryType = function () {
  var startPos = this.state.start,
      startLoc = this.state.startLoc;
  var node = this.startNode();
  var tmp = void 0;
  var type = void 0;
  var isGroupedType = false;

  switch (this.state.type) {
    case _types.types.name:
      return this.flowIdentToTypeAnnotation(startPos, startLoc, node, this.parseIdentifier());

    case _types.types.braceL:
      return this.flowParseObjectType();

    case _types.types.bracketL:
      return this.flowParseTupleType();

    case _types.types.relational:
      if (this.state.value === "<") {
        node.typeParameters = this.flowParseTypeParameterDeclaration();
        this.expect(_types.types.parenL);
        tmp = this.flowParseFunctionTypeParams();
        node.params = tmp.params;
        node.rest = tmp.rest;
        this.expect(_types.types.parenR);

        this.expect(_types.types.arrow);

        node.returnType = this.flowParseType();

        return this.finishNode(node, "FunctionTypeAnnotation");
      }

    case _types.types.parenL:
      this.next();

      // Check to see if this is actually a grouped type
      if (!this.match(_types.types.parenR) && !this.match(_types.types.ellipsis)) {
        if (this.match(_types.types.name)) {
          var token = this.lookahead().type;
          isGroupedType = token !== _types.types.question && token !== _types.types.colon;
        } else {
          isGroupedType = true;
        }
      }

      if (isGroupedType) {
        type = this.flowParseType();
        this.expect(_types.types.parenR);
        return type;
      }

      tmp = this.flowParseFunctionTypeParams();
      node.params = tmp.params;
      node.rest = tmp.rest;

      this.expect(_types.types.parenR);

      this.expect(_types.types.arrow);

      node.returnType = this.flowParseType();
      node.typeParameters = null;

      return this.finishNode(node, "FunctionTypeAnnotation");

    case _types.types.string:
      node.value = this.state.value;
      this.addExtra(node, "rawValue", node.value);
      this.addExtra(node, "raw", this.input.slice(this.state.start, this.state.end));
      this.next();
      return this.finishNode(node, "StringLiteralTypeAnnotation");

    case _types.types._true:case _types.types._false:
      node.value = this.match(_types.types._true);
      this.next();
      return this.finishNode(node, "BooleanLiteralTypeAnnotation");

    case _types.types.plusMin:
      if (this.state.value === "-") {
        this.next();
        if (!this.match(_types.types.num)) this.unexpected();

        node.value = -this.state.value;
        this.addExtra(node, "rawValue", node.value);
        this.addExtra(node, "raw", this.input.slice(this.state.start, this.state.end));
        this.next();
        return this.finishNode(node, "NumericLiteralTypeAnnotation");
      }

    case _types.types.num:
      node.value = this.state.value;
      this.addExtra(node, "rawValue", node.value);
      this.addExtra(node, "raw", this.input.slice(this.state.start, this.state.end));
      this.next();
      return this.finishNode(node, "NumericLiteralTypeAnnotation");

    case _types.types._null:
      node.value = this.match(_types.types._null);
      this.next();
      return this.finishNode(node, "NullLiteralTypeAnnotation");

    case _types.types._this:
      node.value = this.match(_types.types._this);
      this.next();
      return this.finishNode(node, "ThisTypeAnnotation");

    case _types.types.star:
      this.next();
      return this.finishNode(node, "ExistentialTypeParam");

    default:
      if (this.state.type.keyword === "typeof") {
        return this.flowParseTypeofType();
      }
  }

  this.unexpected();
};

pp.flowParsePostfixType = function () {
  var node = this.startNode();
  var type = node.elementType = this.flowParsePrimaryType();
  if (this.match(_types.types.bracketL)) {
    this.expect(_types.types.bracketL);
    this.expect(_types.types.bracketR);
    return this.finishNode(node, "ArrayTypeAnnotation");
  } else {
    return type;
  }
};

pp.flowParsePrefixType = function () {
  var node = this.startNode();
  if (this.eat(_types.types.question)) {
    node.typeAnnotation = this.flowParsePrefixType();
    return this.finishNode(node, "NullableTypeAnnotation");
  } else {
    return this.flowParsePostfixType();
  }
};

pp.flowParseIntersectionType = function () {
  var node = this.startNode();
  var type = this.flowParsePrefixType();
  node.types = [type];
  while (this.eat(_types.types.bitwiseAND)) {
    node.types.push(this.flowParsePrefixType());
  }
  return node.types.length === 1 ? type : this.finishNode(node, "IntersectionTypeAnnotation");
};

pp.flowParseUnionType = function () {
  var node = this.startNode();
  var type = this.flowParseIntersectionType();
  node.types = [type];
  while (this.eat(_types.types.bitwiseOR)) {
    node.types.push(this.flowParseIntersectionType());
  }
  return node.types.length === 1 ? type : this.finishNode(node, "UnionTypeAnnotation");
};

pp.flowParseType = function () {
  var oldInType = this.state.inType;
  this.state.inType = true;
  var type = this.flowParseUnionType();
  this.state.inType = oldInType;
  return type;
};

pp.flowParseTypeAnnotation = function () {
  var node = this.startNode();
  node.typeAnnotation = this.flowParseTypeInitialiser();
  return this.finishNode(node, "TypeAnnotation");
};

pp.flowParseTypeAnnotatableIdentifier = function (requireTypeAnnotation, canBeOptionalParam) {

  var ident = this.parseIdentifier();
  var isOptionalParam = false;

  if (canBeOptionalParam && this.eat(_types.types.question)) {
    this.expect(_types.types.question);
    isOptionalParam = true;
  }

  if (requireTypeAnnotation || this.match(_types.types.colon)) {
    ident.typeAnnotation = this.flowParseTypeAnnotation();
    this.finishNode(ident, ident.type);
  }

  if (isOptionalParam) {
    ident.optional = true;
    this.finishNode(ident, ident.type);
  }

  return ident;
};