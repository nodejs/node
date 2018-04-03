"use strict";

exports.__esModule = true;
exports.default = void 0;

var _includes = _interopRequireDefault(require("lodash/includes"));

var _repeat = _interopRequireDefault(require("lodash/repeat"));

var _renamer = _interopRequireDefault(require("./lib/renamer"));

var _index = _interopRequireDefault(require("../index"));

var _defaults = _interopRequireDefault(require("lodash/defaults"));

var _binding2 = _interopRequireDefault(require("./binding"));

var _globals = _interopRequireDefault(require("globals"));

var t = _interopRequireWildcard(require("@babel/types"));

var _cache = require("../cache");

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _defineProperties(target, props) { for (var i = 0; i < props.length; i++) { var descriptor = props[i]; descriptor.enumerable = descriptor.enumerable || false; descriptor.configurable = true; if ("value" in descriptor) descriptor.writable = true; Object.defineProperty(target, descriptor.key, descriptor); } }

function _createClass(Constructor, protoProps, staticProps) { if (protoProps) _defineProperties(Constructor.prototype, protoProps); if (staticProps) _defineProperties(Constructor, staticProps); return Constructor; }

function gatherNodeParts(node, parts) {
  if (t.isModuleDeclaration(node)) {
    if (node.source) {
      gatherNodeParts(node.source, parts);
    } else if (node.specifiers && node.specifiers.length) {
      var _arr = node.specifiers;

      for (var _i = 0; _i < _arr.length; _i++) {
        var specifier = _arr[_i];
        gatherNodeParts(specifier, parts);
      }
    } else if (node.declaration) {
      gatherNodeParts(node.declaration, parts);
    }
  } else if (t.isModuleSpecifier(node)) {
    gatherNodeParts(node.local, parts);
  } else if (t.isMemberExpression(node)) {
    gatherNodeParts(node.object, parts);
    gatherNodeParts(node.property, parts);
  } else if (t.isIdentifier(node)) {
    parts.push(node.name);
  } else if (t.isLiteral(node)) {
    parts.push(node.value);
  } else if (t.isCallExpression(node)) {
    gatherNodeParts(node.callee, parts);
  } else if (t.isObjectExpression(node) || t.isObjectPattern(node)) {
    var _arr2 = node.properties;

    for (var _i2 = 0; _i2 < _arr2.length; _i2++) {
      var prop = _arr2[_i2];
      gatherNodeParts(prop.key || prop.argument, parts);
    }
  }
}

var collectorVisitor = {
  For: function For(path) {
    var _arr3 = t.FOR_INIT_KEYS;

    for (var _i3 = 0; _i3 < _arr3.length; _i3++) {
      var key = _arr3[_i3];
      var declar = path.get(key);

      if (declar.isVar()) {
        var parentScope = path.scope.getFunctionParent() || path.scope.getProgramParent();
        parentScope.registerBinding("var", declar);
      }
    }
  },
  Declaration: function Declaration(path) {
    if (path.isBlockScoped()) return;

    if (path.isExportDeclaration() && path.get("declaration").isDeclaration()) {
      return;
    }

    var parent = path.scope.getFunctionParent() || path.scope.getProgramParent();
    parent.registerDeclaration(path);
  },
  ReferencedIdentifier: function ReferencedIdentifier(path, state) {
    state.references.push(path);
  },
  ForXStatement: function ForXStatement(path, state) {
    var left = path.get("left");

    if (left.isPattern() || left.isIdentifier()) {
      state.constantViolations.push(path);
    }
  },
  ExportDeclaration: {
    exit: function exit(path) {
      var node = path.node,
          scope = path.scope;
      var declar = node.declaration;

      if (t.isClassDeclaration(declar) || t.isFunctionDeclaration(declar)) {
        var _id = declar.id;
        if (!_id) return;
        var binding = scope.getBinding(_id.name);
        if (binding) binding.reference(path);
      } else if (t.isVariableDeclaration(declar)) {
        var _arr4 = declar.declarations;

        for (var _i4 = 0; _i4 < _arr4.length; _i4++) {
          var decl = _arr4[_i4];
          var ids = t.getBindingIdentifiers(decl);

          for (var name in ids) {
            var _binding = scope.getBinding(name);

            if (_binding) _binding.reference(path);
          }
        }
      }
    }
  },
  LabeledStatement: function LabeledStatement(path) {
    path.scope.getProgramParent().addGlobal(path.node);
    path.scope.getBlockParent().registerDeclaration(path);
  },
  AssignmentExpression: function AssignmentExpression(path, state) {
    state.assignments.push(path);
  },
  UpdateExpression: function UpdateExpression(path, state) {
    state.constantViolations.push(path);
  },
  UnaryExpression: function UnaryExpression(path, state) {
    if (path.node.operator === "delete") {
      state.constantViolations.push(path);
    }
  },
  BlockScoped: function BlockScoped(path) {
    var scope = path.scope;
    if (scope.path === path) scope = scope.parent;
    scope.getBlockParent().registerDeclaration(path);
  },
  ClassDeclaration: function ClassDeclaration(path) {
    var id = path.node.id;
    if (!id) return;
    var name = id.name;
    path.scope.bindings[name] = path.scope.getBinding(name);
  },
  Block: function Block(path) {
    var paths = path.get("body");
    var _arr5 = paths;

    for (var _i5 = 0; _i5 < _arr5.length; _i5++) {
      var bodyPath = _arr5[_i5];

      if (bodyPath.isFunctionDeclaration()) {
        path.scope.getBlockParent().registerDeclaration(bodyPath);
      }
    }
  }
};
var uid = 0;

var Scope = function () {
  function Scope(path) {
    var node = path.node;

    var cached = _cache.scope.get(node);

    if (cached && cached.path === path) {
      return cached;
    }

    _cache.scope.set(node, this);

    this.uid = uid++;
    this.block = node;
    this.path = path;
    this.labels = new Map();
  }

  var _proto = Scope.prototype;

  _proto.traverse = function traverse(node, opts, state) {
    (0, _index.default)(node, opts, this, state, this.path);
  };

  _proto.generateDeclaredUidIdentifier = function generateDeclaredUidIdentifier(name) {
    if (name === void 0) {
      name = "temp";
    }

    var id = this.generateUidIdentifier(name);
    this.push({
      id: id
    });
    return id;
  };

  _proto.generateUidIdentifier = function generateUidIdentifier(name) {
    if (name === void 0) {
      name = "temp";
    }

    return t.identifier(this.generateUid(name));
  };

  _proto.generateUid = function generateUid(name) {
    if (name === void 0) {
      name = "temp";
    }

    name = t.toIdentifier(name).replace(/^_+/, "").replace(/[0-9]+$/g, "");
    var uid;
    var i = 0;

    do {
      uid = this._generateUid(name, i);
      i++;
    } while (this.hasLabel(uid) || this.hasBinding(uid) || this.hasGlobal(uid) || this.hasReference(uid));

    var program = this.getProgramParent();
    program.references[uid] = true;
    program.uids[uid] = true;
    return uid;
  };

  _proto._generateUid = function _generateUid(name, i) {
    var id = name;
    if (i > 1) id += i;
    return "_" + id;
  };

  _proto.generateUidIdentifierBasedOnNode = function generateUidIdentifierBasedOnNode(parent, defaultName) {
    var node = parent;

    if (t.isAssignmentExpression(parent)) {
      node = parent.left;
    } else if (t.isVariableDeclarator(parent)) {
      node = parent.id;
    } else if (t.isObjectProperty(node) || t.isObjectMethod(node)) {
      node = node.key;
    }

    var parts = [];
    gatherNodeParts(node, parts);
    var id = parts.join("$");
    id = id.replace(/^_/, "") || defaultName || "ref";
    return this.generateUidIdentifier(id.slice(0, 20));
  };

  _proto.isStatic = function isStatic(node) {
    if (t.isThisExpression(node) || t.isSuper(node)) {
      return true;
    }

    if (t.isIdentifier(node)) {
      var binding = this.getBinding(node.name);

      if (binding) {
        return binding.constant;
      } else {
        return this.hasBinding(node.name);
      }
    }

    return false;
  };

  _proto.maybeGenerateMemoised = function maybeGenerateMemoised(node, dontPush) {
    if (this.isStatic(node)) {
      return null;
    } else {
      var _id2 = this.generateUidIdentifierBasedOnNode(node);

      if (!dontPush) this.push({
        id: _id2
      });
      return _id2;
    }
  };

  _proto.checkBlockScopedCollisions = function checkBlockScopedCollisions(local, kind, name, id) {
    if (kind === "param") return;
    if (local.kind === "local") return;
    if (kind === "hoisted" && local.kind === "let") return;
    var duplicate = kind === "let" || local.kind === "let" || local.kind === "const" || local.kind === "module" || local.kind === "param" && (kind === "let" || kind === "const");

    if (duplicate) {
      throw this.hub.file.buildCodeFrameError(id, "Duplicate declaration \"" + name + "\"", TypeError);
    }
  };

  _proto.rename = function rename(oldName, newName, block) {
    var binding = this.getBinding(oldName);

    if (binding) {
      newName = newName || this.generateUidIdentifier(oldName).name;
      return new _renamer.default(binding, oldName, newName).rename(block);
    }
  };

  _proto._renameFromMap = function _renameFromMap(map, oldName, newName, value) {
    if (map[oldName]) {
      map[newName] = value;
      map[oldName] = null;
    }
  };

  _proto.dump = function dump() {
    var sep = (0, _repeat.default)("-", 60);
    console.log(sep);
    var scope = this;

    do {
      console.log("#", scope.block.type);

      for (var name in scope.bindings) {
        var binding = scope.bindings[name];
        console.log(" -", name, {
          constant: binding.constant,
          references: binding.references,
          violations: binding.constantViolations.length,
          kind: binding.kind
        });
      }
    } while (scope = scope.parent);

    console.log(sep);
  };

  _proto.toArray = function toArray(node, i) {
    var file = this.hub.file;

    if (t.isIdentifier(node)) {
      var binding = this.getBinding(node.name);

      if (binding && binding.constant && binding.path.isGenericType("Array")) {
        return node;
      }
    }

    if (t.isArrayExpression(node)) {
      return node;
    }

    if (t.isIdentifier(node, {
      name: "arguments"
    })) {
      return t.callExpression(t.memberExpression(t.memberExpression(t.memberExpression(t.identifier("Array"), t.identifier("prototype")), t.identifier("slice")), t.identifier("call")), [node]);
    }

    var helperName = "toArray";
    var args = [node];

    if (i === true) {
      helperName = "toConsumableArray";
    } else if (i) {
      args.push(t.numericLiteral(i));
      helperName = "slicedToArray";
    }

    return t.callExpression(file.addHelper(helperName), args);
  };

  _proto.hasLabel = function hasLabel(name) {
    return !!this.getLabel(name);
  };

  _proto.getLabel = function getLabel(name) {
    return this.labels.get(name);
  };

  _proto.registerLabel = function registerLabel(path) {
    this.labels.set(path.node.label.name, path);
  };

  _proto.registerDeclaration = function registerDeclaration(path) {
    if (path.isFlow()) return;

    if (path.isLabeledStatement()) {
      this.registerLabel(path);
    } else if (path.isFunctionDeclaration()) {
      this.registerBinding("hoisted", path.get("id"), path);
    } else if (path.isVariableDeclaration()) {
      var declarations = path.get("declarations");
      var _arr6 = declarations;

      for (var _i6 = 0; _i6 < _arr6.length; _i6++) {
        var declar = _arr6[_i6];
        this.registerBinding(path.node.kind, declar);
      }
    } else if (path.isClassDeclaration()) {
      this.registerBinding("let", path);
    } else if (path.isImportDeclaration()) {
      var specifiers = path.get("specifiers");
      var _arr7 = specifiers;

      for (var _i7 = 0; _i7 < _arr7.length; _i7++) {
        var specifier = _arr7[_i7];
        this.registerBinding("module", specifier);
      }
    } else if (path.isExportDeclaration()) {
      var _declar = path.get("declaration");

      if (_declar.isClassDeclaration() || _declar.isFunctionDeclaration() || _declar.isVariableDeclaration()) {
        this.registerDeclaration(_declar);
      }
    } else {
      this.registerBinding("unknown", path);
    }
  };

  _proto.buildUndefinedNode = function buildUndefinedNode() {
    if (this.hasBinding("undefined")) {
      return t.unaryExpression("void", t.numericLiteral(0), true);
    } else {
      return t.identifier("undefined");
    }
  };

  _proto.registerConstantViolation = function registerConstantViolation(path) {
    var ids = path.getBindingIdentifiers();

    for (var name in ids) {
      var binding = this.getBinding(name);
      if (binding) binding.reassign(path);
    }
  };

  _proto.registerBinding = function registerBinding(kind, path, bindingPath) {
    if (bindingPath === void 0) {
      bindingPath = path;
    }

    if (!kind) throw new ReferenceError("no `kind`");

    if (path.isVariableDeclaration()) {
      var declarators = path.get("declarations");

      for (var _iterator = declarators, _isArray = Array.isArray(_iterator), _i8 = 0, _iterator = _isArray ? _iterator : _iterator[Symbol.iterator]();;) {
        var _ref;

        if (_isArray) {
          if (_i8 >= _iterator.length) break;
          _ref = _iterator[_i8++];
        } else {
          _i8 = _iterator.next();
          if (_i8.done) break;
          _ref = _i8.value;
        }

        var _declar2 = _ref;
        this.registerBinding(kind, _declar2);
      }

      return;
    }

    var parent = this.getProgramParent();
    var ids = path.getBindingIdentifiers(true);

    for (var name in ids) {
      var _arr8 = ids[name];

      for (var _i9 = 0; _i9 < _arr8.length; _i9++) {
        var _id3 = _arr8[_i9];
        var local = this.getOwnBinding(name);

        if (local) {
          if (local.identifier === _id3) continue;
          this.checkBlockScopedCollisions(local, kind, name, _id3);
        }

        parent.references[name] = true;

        if (local) {
          this.registerConstantViolation(bindingPath);
        } else {
          this.bindings[name] = new _binding2.default({
            identifier: _id3,
            scope: this,
            path: bindingPath,
            kind: kind
          });
        }
      }
    }
  };

  _proto.addGlobal = function addGlobal(node) {
    this.globals[node.name] = node;
  };

  _proto.hasUid = function hasUid(name) {
    var scope = this;

    do {
      if (scope.uids[name]) return true;
    } while (scope = scope.parent);

    return false;
  };

  _proto.hasGlobal = function hasGlobal(name) {
    var scope = this;

    do {
      if (scope.globals[name]) return true;
    } while (scope = scope.parent);

    return false;
  };

  _proto.hasReference = function hasReference(name) {
    var scope = this;

    do {
      if (scope.references[name]) return true;
    } while (scope = scope.parent);

    return false;
  };

  _proto.isPure = function isPure(node, constantsOnly) {
    if (t.isIdentifier(node)) {
      var binding = this.getBinding(node.name);
      if (!binding) return false;
      if (constantsOnly) return binding.constant;
      return true;
    } else if (t.isClass(node)) {
      if (node.superClass && !this.isPure(node.superClass, constantsOnly)) {
        return false;
      }

      return this.isPure(node.body, constantsOnly);
    } else if (t.isClassBody(node)) {
      for (var _iterator2 = node.body, _isArray2 = Array.isArray(_iterator2), _i10 = 0, _iterator2 = _isArray2 ? _iterator2 : _iterator2[Symbol.iterator]();;) {
        var _ref2;

        if (_isArray2) {
          if (_i10 >= _iterator2.length) break;
          _ref2 = _iterator2[_i10++];
        } else {
          _i10 = _iterator2.next();
          if (_i10.done) break;
          _ref2 = _i10.value;
        }

        var _method = _ref2;
        if (!this.isPure(_method, constantsOnly)) return false;
      }

      return true;
    } else if (t.isBinary(node)) {
      return this.isPure(node.left, constantsOnly) && this.isPure(node.right, constantsOnly);
    } else if (t.isArrayExpression(node)) {
      var _arr9 = node.elements;

      for (var _i11 = 0; _i11 < _arr9.length; _i11++) {
        var elem = _arr9[_i11];
        if (!this.isPure(elem, constantsOnly)) return false;
      }

      return true;
    } else if (t.isObjectExpression(node)) {
      var _arr10 = node.properties;

      for (var _i12 = 0; _i12 < _arr10.length; _i12++) {
        var prop = _arr10[_i12];
        if (!this.isPure(prop, constantsOnly)) return false;
      }

      return true;
    } else if (t.isClassMethod(node)) {
      if (node.computed && !this.isPure(node.key, constantsOnly)) return false;
      if (node.kind === "get" || node.kind === "set") return false;
      return true;
    } else if (t.isClassProperty(node) || t.isObjectProperty(node)) {
      if (node.computed && !this.isPure(node.key, constantsOnly)) return false;
      return this.isPure(node.value, constantsOnly);
    } else if (t.isUnaryExpression(node)) {
      return this.isPure(node.argument, constantsOnly);
    } else if (t.isTaggedTemplateExpression(node)) {
      return t.matchesPattern(node.tag, "String.raw") && !this.hasBinding("String", true) && this.isPure(node.quasi, constantsOnly);
    } else if (t.isTemplateLiteral(node)) {
      var _arr11 = node.expressions;

      for (var _i13 = 0; _i13 < _arr11.length; _i13++) {
        var expression = _arr11[_i13];
        if (!this.isPure(expression, constantsOnly)) return false;
      }

      return true;
    } else {
      return t.isPureish(node);
    }
  };

  _proto.setData = function setData(key, val) {
    return this.data[key] = val;
  };

  _proto.getData = function getData(key) {
    var scope = this;

    do {
      var data = scope.data[key];
      if (data != null) return data;
    } while (scope = scope.parent);
  };

  _proto.removeData = function removeData(key) {
    var scope = this;

    do {
      var data = scope.data[key];
      if (data != null) scope.data[key] = null;
    } while (scope = scope.parent);
  };

  _proto.init = function init() {
    if (!this.references) this.crawl();
  };

  _proto.crawl = function crawl() {
    var path = this.path;
    this.references = Object.create(null);
    this.bindings = Object.create(null);
    this.globals = Object.create(null);
    this.uids = Object.create(null);
    this.data = Object.create(null);

    if (path.isLoop()) {
      var _arr12 = t.FOR_INIT_KEYS;

      for (var _i14 = 0; _i14 < _arr12.length; _i14++) {
        var key = _arr12[_i14];
        var node = path.get(key);
        if (node.isBlockScoped()) this.registerBinding(node.node.kind, node);
      }
    }

    if (path.isFunctionExpression() && path.has("id")) {
      if (!path.get("id").node[t.NOT_LOCAL_BINDING]) {
        this.registerBinding("local", path.get("id"), path);
      }
    }

    if (path.isClassExpression() && path.has("id")) {
      if (!path.get("id").node[t.NOT_LOCAL_BINDING]) {
        this.registerBinding("local", path);
      }
    }

    if (path.isFunction()) {
      var params = path.get("params");

      for (var _iterator3 = params, _isArray3 = Array.isArray(_iterator3), _i15 = 0, _iterator3 = _isArray3 ? _iterator3 : _iterator3[Symbol.iterator]();;) {
        var _ref3;

        if (_isArray3) {
          if (_i15 >= _iterator3.length) break;
          _ref3 = _iterator3[_i15++];
        } else {
          _i15 = _iterator3.next();
          if (_i15.done) break;
          _ref3 = _i15.value;
        }

        var _param = _ref3;
        this.registerBinding("param", _param);
      }
    }

    if (path.isCatchClause()) {
      this.registerBinding("let", path);
    }

    var parent = this.getProgramParent();
    if (parent.crawling) return;
    var state = {
      references: [],
      constantViolations: [],
      assignments: []
    };
    this.crawling = true;
    path.traverse(collectorVisitor, state);
    this.crawling = false;

    for (var _iterator4 = state.assignments, _isArray4 = Array.isArray(_iterator4), _i16 = 0, _iterator4 = _isArray4 ? _iterator4 : _iterator4[Symbol.iterator]();;) {
      var _ref4;

      if (_isArray4) {
        if (_i16 >= _iterator4.length) break;
        _ref4 = _iterator4[_i16++];
      } else {
        _i16 = _iterator4.next();
        if (_i16.done) break;
        _ref4 = _i16.value;
      }

      var _path3 = _ref4;

      var ids = _path3.getBindingIdentifiers();

      var programParent = void 0;

      for (var name in ids) {
        if (_path3.scope.getBinding(name)) continue;
        programParent = programParent || _path3.scope.getProgramParent();
        programParent.addGlobal(ids[name]);
      }

      _path3.scope.registerConstantViolation(_path3);
    }

    for (var _iterator5 = state.references, _isArray5 = Array.isArray(_iterator5), _i17 = 0, _iterator5 = _isArray5 ? _iterator5 : _iterator5[Symbol.iterator]();;) {
      var _ref5;

      if (_isArray5) {
        if (_i17 >= _iterator5.length) break;
        _ref5 = _iterator5[_i17++];
      } else {
        _i17 = _iterator5.next();
        if (_i17.done) break;
        _ref5 = _i17.value;
      }

      var _ref7 = _ref5;

      var binding = _ref7.scope.getBinding(_ref7.node.name);

      if (binding) {
        binding.reference(_ref7);
      } else {
        _ref7.scope.getProgramParent().addGlobal(_ref7.node);
      }
    }

    for (var _iterator6 = state.constantViolations, _isArray6 = Array.isArray(_iterator6), _i18 = 0, _iterator6 = _isArray6 ? _iterator6 : _iterator6[Symbol.iterator]();;) {
      var _ref6;

      if (_isArray6) {
        if (_i18 >= _iterator6.length) break;
        _ref6 = _iterator6[_i18++];
      } else {
        _i18 = _iterator6.next();
        if (_i18.done) break;
        _ref6 = _i18.value;
      }

      var _path4 = _ref6;

      _path4.scope.registerConstantViolation(_path4);
    }
  };

  _proto.push = function push(opts) {
    var path = this.path;

    if (!path.isBlockStatement() && !path.isProgram()) {
      path = this.getBlockParent().path;
    }

    if (path.isSwitchStatement()) {
      path = (this.getFunctionParent() || this.getProgramParent()).path;
    }

    if (path.isLoop() || path.isCatchClause() || path.isFunction()) {
      path.ensureBlock();
      path = path.get("body");
    }

    var unique = opts.unique;
    var kind = opts.kind || "var";
    var blockHoist = opts._blockHoist == null ? 2 : opts._blockHoist;
    var dataKey = "declaration:" + kind + ":" + blockHoist;
    var declarPath = !unique && path.getData(dataKey);

    if (!declarPath) {
      var declar = t.variableDeclaration(kind, []);
      declar._blockHoist = blockHoist;

      var _path$unshiftContaine = path.unshiftContainer("body", [declar]);

      declarPath = _path$unshiftContaine[0];
      if (!unique) path.setData(dataKey, declarPath);
    }

    var declarator = t.variableDeclarator(opts.id, opts.init);
    declarPath.node.declarations.push(declarator);
    this.registerBinding(kind, declarPath.get("declarations").pop());
  };

  _proto.getProgramParent = function getProgramParent() {
    var scope = this;

    do {
      if (scope.path.isProgram()) {
        return scope;
      }
    } while (scope = scope.parent);

    throw new Error("Couldn't find a Program");
  };

  _proto.getFunctionParent = function getFunctionParent() {
    var scope = this;

    do {
      if (scope.path.isFunctionParent()) {
        return scope;
      }
    } while (scope = scope.parent);

    return null;
  };

  _proto.getBlockParent = function getBlockParent() {
    var scope = this;

    do {
      if (scope.path.isBlockParent()) {
        return scope;
      }
    } while (scope = scope.parent);

    throw new Error("We couldn't find a BlockStatement, For, Switch, Function, Loop or Program...");
  };

  _proto.getAllBindings = function getAllBindings() {
    var ids = Object.create(null);
    var scope = this;

    do {
      (0, _defaults.default)(ids, scope.bindings);
      scope = scope.parent;
    } while (scope);

    return ids;
  };

  _proto.getAllBindingsOfKind = function getAllBindingsOfKind() {
    var ids = Object.create(null);
    var _arr13 = arguments;

    for (var _i19 = 0; _i19 < _arr13.length; _i19++) {
      var kind = _arr13[_i19];
      var scope = this;

      do {
        for (var name in scope.bindings) {
          var binding = scope.bindings[name];
          if (binding.kind === kind) ids[name] = binding;
        }

        scope = scope.parent;
      } while (scope);
    }

    return ids;
  };

  _proto.bindingIdentifierEquals = function bindingIdentifierEquals(name, node) {
    return this.getBindingIdentifier(name) === node;
  };

  _proto.getBinding = function getBinding(name) {
    var scope = this;

    do {
      var binding = scope.getOwnBinding(name);
      if (binding) return binding;
    } while (scope = scope.parent);
  };

  _proto.getOwnBinding = function getOwnBinding(name) {
    return this.bindings[name];
  };

  _proto.getBindingIdentifier = function getBindingIdentifier(name) {
    var info = this.getBinding(name);
    return info && info.identifier;
  };

  _proto.getOwnBindingIdentifier = function getOwnBindingIdentifier(name) {
    var binding = this.bindings[name];
    return binding && binding.identifier;
  };

  _proto.hasOwnBinding = function hasOwnBinding(name) {
    return !!this.getOwnBinding(name);
  };

  _proto.hasBinding = function hasBinding(name, noGlobals) {
    if (!name) return false;
    if (this.hasOwnBinding(name)) return true;
    if (this.parentHasBinding(name, noGlobals)) return true;
    if (this.hasUid(name)) return true;
    if (!noGlobals && (0, _includes.default)(Scope.globals, name)) return true;
    if (!noGlobals && (0, _includes.default)(Scope.contextVariables, name)) return true;
    return false;
  };

  _proto.parentHasBinding = function parentHasBinding(name, noGlobals) {
    return this.parent && this.parent.hasBinding(name, noGlobals);
  };

  _proto.moveBindingTo = function moveBindingTo(name, scope) {
    var info = this.getBinding(name);

    if (info) {
      info.scope.removeOwnBinding(name);
      info.scope = scope;
      scope.bindings[name] = info;
    }
  };

  _proto.removeOwnBinding = function removeOwnBinding(name) {
    delete this.bindings[name];
  };

  _proto.removeBinding = function removeBinding(name) {
    var info = this.getBinding(name);

    if (info) {
      info.scope.removeOwnBinding(name);
    }

    var scope = this;

    do {
      if (scope.uids[name]) {
        scope.uids[name] = false;
      }
    } while (scope = scope.parent);
  };

  _createClass(Scope, [{
    key: "parent",
    get: function get() {
      var parent = this.path.findParent(function (p) {
        return p.isScope();
      });
      return parent && parent.scope;
    }
  }, {
    key: "parentBlock",
    get: function get() {
      return this.path.parent;
    }
  }, {
    key: "hub",
    get: function get() {
      return this.path.hub;
    }
  }]);

  return Scope;
}();

exports.default = Scope;
Scope.globals = Object.keys(_globals.default.builtin);
Scope.contextVariables = ["arguments", "undefined", "Infinity", "NaN"];