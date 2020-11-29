"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = exports.SHOULD_SKIP = exports.SHOULD_STOP = exports.REMOVED = void 0;

var virtualTypes = _interopRequireWildcard(require("./lib/virtual-types"));

var _debug = _interopRequireDefault(require("debug"));

var _index = _interopRequireDefault(require("../index"));

var _scope = _interopRequireDefault(require("../scope"));

var t = _interopRequireWildcard(require("@babel/types"));

var _cache = require("../cache");

var _generator = _interopRequireDefault(require("@babel/generator"));

var NodePath_ancestry = _interopRequireWildcard(require("./ancestry"));

var NodePath_inference = _interopRequireWildcard(require("./inference"));

var NodePath_replacement = _interopRequireWildcard(require("./replacement"));

var NodePath_evaluation = _interopRequireWildcard(require("./evaluation"));

var NodePath_conversion = _interopRequireWildcard(require("./conversion"));

var NodePath_introspection = _interopRequireWildcard(require("./introspection"));

var NodePath_context = _interopRequireWildcard(require("./context"));

var NodePath_removal = _interopRequireWildcard(require("./removal"));

var NodePath_modification = _interopRequireWildcard(require("./modification"));

var NodePath_family = _interopRequireWildcard(require("./family"));

var NodePath_comments = _interopRequireWildcard(require("./comments"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _getRequireWildcardCache() { if (typeof WeakMap !== "function") return null; var cache = new WeakMap(); _getRequireWildcardCache = function () { return cache; }; return cache; }

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } if (obj === null || typeof obj !== "object" && typeof obj !== "function") { return { default: obj }; } var cache = _getRequireWildcardCache(); if (cache && cache.has(obj)) { return cache.get(obj); } var newObj = {}; var hasPropertyDescriptor = Object.defineProperty && Object.getOwnPropertyDescriptor; for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = hasPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : null; if (desc && (desc.get || desc.set)) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } newObj.default = obj; if (cache) { cache.set(obj, newObj); } return newObj; }

const debug = (0, _debug.default)("babel");
const REMOVED = 1 << 0;
exports.REMOVED = REMOVED;
const SHOULD_STOP = 1 << 1;
exports.SHOULD_STOP = SHOULD_STOP;
const SHOULD_SKIP = 1 << 2;
exports.SHOULD_SKIP = SHOULD_SKIP;

class NodePath {
  constructor(hub, parent) {
    this.contexts = [];
    this.state = null;
    this.opts = null;
    this._traverseFlags = 0;
    this.skipKeys = null;
    this.parentPath = null;
    this.container = null;
    this.listKey = null;
    this.key = null;
    this.node = null;
    this.type = null;
    this.parent = parent;
    this.hub = hub;
    this.data = null;
    this.context = null;
    this.scope = null;
  }

  static get({
    hub,
    parentPath,
    parent,
    container,
    listKey,
    key
  }) {
    if (!hub && parentPath) {
      hub = parentPath.hub;
    }

    if (!parent) {
      throw new Error("To get a node path the parent needs to exist");
    }

    const targetNode = container[key];

    let paths = _cache.path.get(parent);

    if (!paths) {
      paths = new Map();

      _cache.path.set(parent, paths);
    }

    let path = paths.get(targetNode);

    if (!path) {
      path = new NodePath(hub, parent);
      if (targetNode) paths.set(targetNode, path);
    }

    path.setup(parentPath, container, listKey, key);
    return path;
  }

  getScope(scope) {
    return this.isScope() ? new _scope.default(this) : scope;
  }

  setData(key, val) {
    if (this.data == null) {
      this.data = Object.create(null);
    }

    return this.data[key] = val;
  }

  getData(key, def) {
    if (this.data == null) {
      this.data = Object.create(null);
    }

    let val = this.data[key];
    if (val === undefined && def !== undefined) val = this.data[key] = def;
    return val;
  }

  buildCodeFrameError(msg, Error = SyntaxError) {
    return this.hub.buildError(this.node, msg, Error);
  }

  traverse(visitor, state) {
    (0, _index.default)(this.node, visitor, this.scope, state, this);
  }

  set(key, node) {
    t.validate(this.node, key, node);
    this.node[key] = node;
  }

  getPathLocation() {
    const parts = [];
    let path = this;

    do {
      let key = path.key;
      if (path.inList) key = `${path.listKey}[${key}]`;
      parts.unshift(key);
    } while (path = path.parentPath);

    return parts.join(".");
  }

  debug(message) {
    if (!debug.enabled) return;
    debug(`${this.getPathLocation()} ${this.type}: ${message}`);
  }

  toString() {
    return (0, _generator.default)(this.node).code;
  }

  get inList() {
    return !!this.listKey;
  }

  set inList(inList) {
    if (!inList) {
      this.listKey = null;
    }
  }

  get parentKey() {
    return this.listKey || this.key;
  }

  get shouldSkip() {
    return !!(this._traverseFlags & SHOULD_SKIP);
  }

  set shouldSkip(v) {
    if (v) {
      this._traverseFlags |= SHOULD_SKIP;
    } else {
      this._traverseFlags &= ~SHOULD_SKIP;
    }
  }

  get shouldStop() {
    return !!(this._traverseFlags & SHOULD_STOP);
  }

  set shouldStop(v) {
    if (v) {
      this._traverseFlags |= SHOULD_STOP;
    } else {
      this._traverseFlags &= ~SHOULD_STOP;
    }
  }

  get removed() {
    return !!(this._traverseFlags & REMOVED);
  }

  set removed(v) {
    if (v) {
      this._traverseFlags |= REMOVED;
    } else {
      this._traverseFlags &= ~REMOVED;
    }
  }

}

exports.default = NodePath;
Object.assign(NodePath.prototype, NodePath_ancestry, NodePath_inference, NodePath_replacement, NodePath_evaluation, NodePath_conversion, NodePath_introspection, NodePath_context, NodePath_removal, NodePath_modification, NodePath_family, NodePath_comments);

for (const type of t.TYPES) {
  const typeKey = `is${type}`;
  const fn = t[typeKey];

  NodePath.prototype[typeKey] = function (opts) {
    return fn(this.node, opts);
  };

  NodePath.prototype[`assert${type}`] = function (opts) {
    if (!fn(this.node, opts)) {
      throw new TypeError(`Expected node path of type ${type}`);
    }
  };
}

for (const type of Object.keys(virtualTypes)) {
  if (type[0] === "_") continue;
  if (t.TYPES.indexOf(type) < 0) t.TYPES.push(type);
  const virtualType = virtualTypes[type];

  NodePath.prototype[`is${type}`] = function (opts) {
    return virtualType.checkPath(this, opts);
  };
}