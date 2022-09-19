"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = exports.SHOULD_STOP = exports.SHOULD_SKIP = exports.REMOVED = void 0;

var virtualTypes = require("./lib/virtual-types");

var _debug = require("debug");

var _index = require("../index");

var _scope = require("../scope");

var _t = require("@babel/types");

var t = _t;

var _cache = require("../cache");

var _generator = require("@babel/generator");

var NodePath_ancestry = require("./ancestry");

var NodePath_inference = require("./inference");

var NodePath_replacement = require("./replacement");

var NodePath_evaluation = require("./evaluation");

var NodePath_conversion = require("./conversion");

var NodePath_introspection = require("./introspection");

var NodePath_context = require("./context");

var NodePath_removal = require("./removal");

var NodePath_modification = require("./modification");

var NodePath_family = require("./family");

var NodePath_comments = require("./comments");

var NodePath_virtual_types_validator = require("./lib/virtual-types-validator");

const {
  validate
} = _t;

const debug = _debug("babel");

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

  hasNode() {
    return this.node != null;
  }

  buildCodeFrameError(msg, Error = SyntaxError) {
    return this.hub.buildError(this.node, msg, Error);
  }

  traverse(visitor, state) {
    (0, _index.default)(this.node, visitor, this.scope, state, this);
  }

  set(key, node) {
    validate(this.node, key, node);
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

Object.assign(NodePath.prototype, NodePath_ancestry, NodePath_inference, NodePath_replacement, NodePath_evaluation, NodePath_conversion, NodePath_introspection, NodePath_context, NodePath_removal, NodePath_modification, NodePath_family, NodePath_comments);
{
  NodePath.prototype._guessExecutionStatusRelativeToDifferentFunctions = NodePath_introspection._guessExecutionStatusRelativeTo;
}

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

Object.assign(NodePath.prototype, NodePath_virtual_types_validator);

for (const type of Object.keys(virtualTypes)) {
  if (type[0] === "_") continue;
  if (!t.TYPES.includes(type)) t.TYPES.push(type);
}

var _default = NodePath;
exports.default = _default;

//# sourceMappingURL=index.js.map
