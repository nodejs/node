function _classPrivateFieldInitSpec(obj, privateMap, value) { _checkPrivateRedeclaration(obj, privateMap); privateMap.set(obj, value); }

function _checkPrivateRedeclaration(obj, privateCollection) { if (privateCollection.has(obj)) { throw new TypeError("Cannot initialize the same private elements twice on an object"); } }

function _classPrivateFieldGet(receiver, privateMap) { var descriptor = _classExtractFieldDescriptor(receiver, privateMap, "get"); return _classApplyDescriptorGet(receiver, descriptor); }

function _classApplyDescriptorGet(receiver, descriptor) { if (descriptor.get) { return descriptor.get.call(receiver); } return descriptor.value; }

function _classPrivateFieldSet(receiver, privateMap, value) { var descriptor = _classExtractFieldDescriptor(receiver, privateMap, "set"); _classApplyDescriptorSet(receiver, descriptor, value); return value; }

function _classExtractFieldDescriptor(receiver, privateMap, action) { if (!privateMap.has(receiver)) { throw new TypeError("attempted to " + action + " private field on non-instance"); } return privateMap.get(receiver); }

function _classApplyDescriptorSet(receiver, descriptor, value) { if (descriptor.set) { descriptor.set.call(receiver, value); } else { if (!descriptor.writable) { throw new TypeError("attempted to set read only private field"); } descriptor.value = value; } }

const {
  Definition,
  PatternVisitor: OriginalPatternVisitor,
  Referencer: OriginalReferencer,
  Scope,
  ScopeManager
} = require("@nicolo-ribaudo/eslint-scope-5-internals");

const {
  getKeys: fallback
} = require("eslint-visitor-keys");

let visitorKeysMap;

function getVisitorValues(nodeType, client) {
  if (visitorKeysMap) return visitorKeysMap[nodeType];
  const {
    FLOW_FLIPPED_ALIAS_KEYS,
    VISITOR_KEYS
  } = client.getTypesInfo();
  const flowFlippedAliasKeys = FLOW_FLIPPED_ALIAS_KEYS.concat(["ArrayPattern", "ClassDeclaration", "ClassExpression", "FunctionDeclaration", "FunctionExpression", "Identifier", "ObjectPattern", "RestElement"]);
  visitorKeysMap = Object.entries(VISITOR_KEYS).reduce((acc, [key, value]) => {
    if (!flowFlippedAliasKeys.includes(value)) {
      acc[key] = value;
    }

    return acc;
  }, {});
  return visitorKeysMap[nodeType];
}

const propertyTypes = {
  callProperties: {
    type: "loop",
    values: ["value"]
  },
  indexers: {
    type: "loop",
    values: ["key", "value"]
  },
  properties: {
    type: "loop",
    values: ["argument", "value"]
  },
  types: {
    type: "loop"
  },
  params: {
    type: "loop"
  },
  argument: {
    type: "single"
  },
  elementType: {
    type: "single"
  },
  qualification: {
    type: "single"
  },
  rest: {
    type: "single"
  },
  returnType: {
    type: "single"
  },
  typeAnnotation: {
    type: "typeAnnotation"
  },
  typeParameters: {
    type: "typeParameters"
  },
  id: {
    type: "id"
  }
};

class PatternVisitor extends OriginalPatternVisitor {
  ArrayPattern(node) {
    node.elements.forEach(this.visit, this);
  }

  ObjectPattern(node) {
    node.properties.forEach(this.visit, this);
  }

}

var _client = new WeakMap();

class Referencer extends OriginalReferencer {
  constructor(options, scopeManager, client) {
    super(options, scopeManager);

    _classPrivateFieldInitSpec(this, _client, {
      writable: true,
      value: void 0
    });

    _classPrivateFieldSet(this, _client, client);
  }

  visitPattern(node, options, callback) {
    if (!node) {
      return;
    }

    this._checkIdentifierOrVisit(node.typeAnnotation);

    if (node.type === "AssignmentPattern") {
      this._checkIdentifierOrVisit(node.left.typeAnnotation);
    }

    if (typeof options === "function") {
      callback = options;
      options = {
        processRightHandNodes: false
      };
    }

    const visitor = new PatternVisitor(this.options, node, callback);
    visitor.visit(node);

    if (options.processRightHandNodes) {
      visitor.rightHandNodes.forEach(this.visit, this);
    }
  }

  visitClass(node) {
    this._visitArray(node.decorators);

    const typeParamScope = this._nestTypeParamScope(node);

    this._visitTypeAnnotation(node.implements);

    this._visitTypeAnnotation(node.superTypeParameters && node.superTypeParameters.params);

    super.visitClass(node);

    if (typeParamScope) {
      this.close(node);
    }
  }

  visitFunction(node) {
    const typeParamScope = this._nestTypeParamScope(node);

    this._checkIdentifierOrVisit(node.returnType);

    super.visitFunction(node);

    if (typeParamScope) {
      this.close(node);
    }
  }

  visitProperty(node) {
    var _node$value;

    if (((_node$value = node.value) == null ? void 0 : _node$value.type) === "TypeCastExpression") {
      this._visitTypeAnnotation(node.value);
    }

    this._visitArray(node.decorators);

    super.visitProperty(node);
  }

  InterfaceDeclaration(node) {
    this._createScopeVariable(node, node.id);

    const typeParamScope = this._nestTypeParamScope(node);

    this._visitArray(node.extends);

    this.visit(node.body);

    if (typeParamScope) {
      this.close(node);
    }
  }

  TypeAlias(node) {
    this._createScopeVariable(node, node.id);

    const typeParamScope = this._nestTypeParamScope(node);

    this.visit(node.right);

    if (typeParamScope) {
      this.close(node);
    }
  }

  ClassProperty(node) {
    this._visitClassProperty(node);
  }

  ClassPrivateProperty(node) {
    this._visitClassProperty(node);
  }

  PropertyDefinition(node) {
    this._visitClassProperty(node);
  }

  ClassPrivateMethod(node) {
    super.MethodDefinition(node);
  }

  DeclareModule(node) {
    this._visitDeclareX(node);
  }

  DeclareFunction(node) {
    this._visitDeclareX(node);
  }

  DeclareVariable(node) {
    this._visitDeclareX(node);
  }

  DeclareClass(node) {
    this._visitDeclareX(node);
  }

  OptionalMemberExpression(node) {
    super.MemberExpression(node);
  }

  _visitClassProperty(node) {
    this._visitTypeAnnotation(node.typeAnnotation);

    this.visitProperty(node);
  }

  _visitDeclareX(node) {
    if (node.id) {
      this._createScopeVariable(node, node.id);
    }

    const typeParamScope = this._nestTypeParamScope(node);

    if (typeParamScope) {
      this.close(node);
    }
  }

  _createScopeVariable(node, name) {
    this.currentScope().variableScope.__define(name, new Definition("Variable", name, node, null, null, null));
  }

  _nestTypeParamScope(node) {
    if (!node.typeParameters) {
      return null;
    }

    const parentScope = this.scopeManager.__currentScope;
    const scope = new Scope(this.scopeManager, "type-parameters", parentScope, node, false);

    this.scopeManager.__nestScope(scope);

    for (let j = 0; j < node.typeParameters.params.length; j++) {
      const name = node.typeParameters.params[j];

      scope.__define(name, new Definition("TypeParameter", name, name));

      if (name.typeAnnotation) {
        this._checkIdentifierOrVisit(name);
      }
    }

    scope.__define = function () {
      return parentScope.__define.apply(parentScope, arguments);
    };

    return scope;
  }

  _visitTypeAnnotation(node) {
    if (!node) {
      return;
    }

    if (Array.isArray(node)) {
      node.forEach(this._visitTypeAnnotation, this);
      return;
    }

    const visitorValues = getVisitorValues(node.type, _classPrivateFieldGet(this, _client));

    if (!visitorValues) {
      return;
    }

    for (let i = 0; i < visitorValues.length; i++) {
      const visitorValue = visitorValues[i];
      const propertyType = propertyTypes[visitorValue];
      const nodeProperty = node[visitorValue];

      if (propertyType == null || nodeProperty == null) {
        continue;
      }

      if (propertyType.type === "loop") {
        for (let j = 0; j < nodeProperty.length; j++) {
          if (Array.isArray(propertyType.values)) {
            for (let k = 0; k < propertyType.values.length; k++) {
              const loopPropertyNode = nodeProperty[j][propertyType.values[k]];

              if (loopPropertyNode) {
                this._checkIdentifierOrVisit(loopPropertyNode);
              }
            }
          } else {
            this._checkIdentifierOrVisit(nodeProperty[j]);
          }
        }
      } else if (propertyType.type === "single") {
        this._checkIdentifierOrVisit(nodeProperty);
      } else if (propertyType.type === "typeAnnotation") {
        this._visitTypeAnnotation(node.typeAnnotation);
      } else if (propertyType.type === "typeParameters") {
        for (let l = 0; l < node.typeParameters.params.length; l++) {
          this._checkIdentifierOrVisit(node.typeParameters.params[l]);
        }
      } else if (propertyType.type === "id") {
        if (node.id.type === "Identifier") {
          this._checkIdentifierOrVisit(node.id);
        } else {
          this._visitTypeAnnotation(node.id);
        }
      }
    }
  }

  _checkIdentifierOrVisit(node) {
    if (node != null && node.typeAnnotation) {
      this._visitTypeAnnotation(node.typeAnnotation);
    } else if ((node == null ? void 0 : node.type) === "Identifier") {
      this.visit(node);
    } else {
      this._visitTypeAnnotation(node);
    }
  }

  _visitArray(nodeList) {
    if (nodeList) {
      for (const node of nodeList) {
        this.visit(node);
      }
    }
  }

}

module.exports = function analyzeScope(ast, parserOptions, client) {
  var _parserOptions$ecmaFe;

  const options = {
    ignoreEval: true,
    optimistic: false,
    directive: false,
    nodejsScope: ast.sourceType === "script" && ((_parserOptions$ecmaFe = parserOptions.ecmaFeatures) == null ? void 0 : _parserOptions$ecmaFe.globalReturn) === true,
    impliedStrict: false,
    sourceType: ast.sourceType,
    ecmaVersion: parserOptions.ecmaVersion,
    fallback
  };
  options.childVisitorKeys = client.getVisitorKeys();
  const scopeManager = new ScopeManager(options);
  const referencer = new Referencer(options, scopeManager, client);
  referencer.visit(ast);
  return scopeManager;
};

//# sourceMappingURL=analyze-scope.cjs.map
