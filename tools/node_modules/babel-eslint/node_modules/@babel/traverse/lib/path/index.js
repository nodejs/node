"use strict";

exports.__esModule = true;
exports.default = void 0;

var virtualTypes = _interopRequireWildcard(require("./lib/virtual-types"));

var _debug2 = _interopRequireDefault(require("debug"));

var _invariant = _interopRequireDefault(require("invariant"));

var _index = _interopRequireDefault(require("../index"));

var _scope = _interopRequireDefault(require("../scope"));

var t = _interopRequireWildcard(require("@babel/types"));

var _cache = require("../cache");

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

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

var _debug = (0, _debug2.default)("babel");

var NodePath = function () {
  function NodePath(hub, parent) {
    this.parent = void 0;
    this.hub = void 0;
    this.contexts = void 0;
    this.data = void 0;
    this.shouldSkip = void 0;
    this.shouldStop = void 0;
    this.removed = void 0;
    this.state = void 0;
    this.opts = void 0;
    this.skipKeys = void 0;
    this.parentPath = void 0;
    this.context = void 0;
    this.container = void 0;
    this.listKey = void 0;
    this.inList = void 0;
    this.parentKey = void 0;
    this.key = void 0;
    this.node = void 0;
    this.scope = void 0;
    this.type = void 0;
    this.typeAnnotation = void 0;
    this.parent = parent;
    this.hub = hub;
    this.contexts = [];
    this.data = {};
    this.shouldSkip = false;
    this.shouldStop = false;
    this.removed = false;
    this.state = null;
    this.opts = null;
    this.skipKeys = null;
    this.parentPath = null;
    this.context = null;
    this.container = null;
    this.listKey = null;
    this.inList = false;
    this.parentKey = null;
    this.key = null;
    this.node = null;
    this.scope = null;
    this.type = null;
    this.typeAnnotation = null;
  }

  NodePath.get = function get(_ref) {
    var hub = _ref.hub,
        parentPath = _ref.parentPath,
        parent = _ref.parent,
        container = _ref.container,
        listKey = _ref.listKey,
        key = _ref.key;

    if (!hub && parentPath) {
      hub = parentPath.hub;
    }

    (0, _invariant.default)(parent, "To get a node path the parent needs to exist");
    var targetNode = container[key];
    var paths = _cache.path.get(parent) || [];

    if (!_cache.path.has(parent)) {
      _cache.path.set(parent, paths);
    }

    var path;

    for (var i = 0; i < paths.length; i++) {
      var pathCheck = paths[i];

      if (pathCheck.node === targetNode) {
        path = pathCheck;
        break;
      }
    }

    if (!path) {
      path = new NodePath(hub, parent);
      paths.push(path);
    }

    path.setup(parentPath, container, listKey, key);
    return path;
  };

  var _proto = NodePath.prototype;

  _proto.getScope = function getScope(scope) {
    return this.isScope() ? new _scope.default(this) : scope;
  };

  _proto.setData = function setData(key, val) {
    return this.data[key] = val;
  };

  _proto.getData = function getData(key, def) {
    var val = this.data[key];
    if (!val && def) val = this.data[key] = def;
    return val;
  };

  _proto.buildCodeFrameError = function buildCodeFrameError(msg, Error) {
    if (Error === void 0) {
      Error = SyntaxError;
    }

    return this.hub.file.buildCodeFrameError(this.node, msg, Error);
  };

  _proto.traverse = function traverse(visitor, state) {
    (0, _index.default)(this.node, visitor, this.scope, state, this);
  };

  _proto.set = function set(key, node) {
    t.validate(this.node, key, node);
    this.node[key] = node;
  };

  _proto.getPathLocation = function getPathLocation() {
    var parts = [];
    var path = this;

    do {
      var key = path.key;
      if (path.inList) key = path.listKey + "[" + key + "]";
      parts.unshift(key);
    } while (path = path.parentPath);

    return parts.join(".");
  };

  _proto.debug = function debug(message) {
    if (!_debug.enabled) return;

    _debug(this.getPathLocation() + " " + this.type + ": " + message);
  };

  return NodePath;
}();

exports.default = NodePath;
Object.assign(NodePath.prototype, NodePath_ancestry, NodePath_inference, NodePath_replacement, NodePath_evaluation, NodePath_conversion, NodePath_introspection, NodePath_context, NodePath_removal, NodePath_modification, NodePath_family, NodePath_comments);

var _loop = function _loop(type) {
  var typeKey = "is" + type;

  NodePath.prototype[typeKey] = function (opts) {
    return t[typeKey](this.node, opts);
  };

  NodePath.prototype["assert" + type] = function (opts) {
    if (!this[typeKey](opts)) {
      throw new TypeError("Expected node path of type " + type);
    }
  };
};

var _arr = t.TYPES;

for (var _i = 0; _i < _arr.length; _i++) {
  var type = _arr[_i];

  _loop(type);
}

var _loop2 = function _loop2(type) {
  if (type[0] === "_") return "continue";
  if (t.TYPES.indexOf(type) < 0) t.TYPES.push(type);
  var virtualType = virtualTypes[type];

  NodePath.prototype["is" + type] = function (opts) {
    return virtualType.checkPath(this, opts);
  };
};

for (var type in virtualTypes) {
  var _ret = _loop2(type);

  if (_ret === "continue") continue;
}