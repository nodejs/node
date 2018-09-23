"use strict";

exports.__esModule = true;
exports.default = void 0;

var _path4 = _interopRequireDefault(require("./path"));

var t = _interopRequireWildcard(require("@babel/types"));

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

var testing = process.env.NODE_ENV === "test";

var TraversalContext = function () {
  function TraversalContext(scope, opts, state, parentPath) {
    this.parentPath = void 0;
    this.scope = void 0;
    this.state = void 0;
    this.opts = void 0;
    this.queue = null;
    this.parentPath = parentPath;
    this.scope = scope;
    this.state = state;
    this.opts = opts;
  }

  var _proto = TraversalContext.prototype;

  _proto.shouldVisit = function shouldVisit(node) {
    var opts = this.opts;
    if (opts.enter || opts.exit) return true;
    if (opts[node.type]) return true;
    var keys = t.VISITOR_KEYS[node.type];
    if (!keys || !keys.length) return false;

    for (var _iterator = keys, _isArray = Array.isArray(_iterator), _i = 0, _iterator = _isArray ? _iterator : _iterator[Symbol.iterator]();;) {
      var _ref;

      if (_isArray) {
        if (_i >= _iterator.length) break;
        _ref = _iterator[_i++];
      } else {
        _i = _iterator.next();
        if (_i.done) break;
        _ref = _i.value;
      }

      var _key = _ref;
      if (node[_key]) return true;
    }

    return false;
  };

  _proto.create = function create(node, obj, key, listKey) {
    return _path4.default.get({
      parentPath: this.parentPath,
      parent: node,
      container: obj,
      key: key,
      listKey: listKey
    });
  };

  _proto.maybeQueue = function maybeQueue(path, notPriority) {
    if (this.trap) {
      throw new Error("Infinite cycle detected");
    }

    if (this.queue) {
      if (notPriority) {
        this.queue.push(path);
      } else {
        this.priorityQueue.push(path);
      }
    }
  };

  _proto.visitMultiple = function visitMultiple(container, parent, listKey) {
    if (container.length === 0) return false;
    var queue = [];

    for (var key = 0; key < container.length; key++) {
      var node = container[key];

      if (node && this.shouldVisit(node)) {
        queue.push(this.create(parent, container, key, listKey));
      }
    }

    return this.visitQueue(queue);
  };

  _proto.visitSingle = function visitSingle(node, key) {
    if (this.shouldVisit(node[key])) {
      return this.visitQueue([this.create(node, node, key)]);
    } else {
      return false;
    }
  };

  _proto.visitQueue = function visitQueue(queue) {
    this.queue = queue;
    this.priorityQueue = [];
    var visited = [];
    var stop = false;

    for (var _iterator2 = queue, _isArray2 = Array.isArray(_iterator2), _i2 = 0, _iterator2 = _isArray2 ? _iterator2 : _iterator2[Symbol.iterator]();;) {
      var _ref2;

      if (_isArray2) {
        if (_i2 >= _iterator2.length) break;
        _ref2 = _iterator2[_i2++];
      } else {
        _i2 = _iterator2.next();
        if (_i2.done) break;
        _ref2 = _i2.value;
      }

      var _path2 = _ref2;

      _path2.resync();

      if (_path2.contexts.length === 0 || _path2.contexts[_path2.contexts.length - 1] !== this) {
        _path2.pushContext(this);
      }

      if (_path2.key === null) continue;

      if (testing && queue.length >= 10000) {
        this.trap = true;
      }

      if (visited.indexOf(_path2.node) >= 0) continue;
      visited.push(_path2.node);

      if (_path2.visit()) {
        stop = true;
        break;
      }

      if (this.priorityQueue.length) {
        stop = this.visitQueue(this.priorityQueue);
        this.priorityQueue = [];
        this.queue = queue;
        if (stop) break;
      }
    }

    for (var _iterator3 = queue, _isArray3 = Array.isArray(_iterator3), _i3 = 0, _iterator3 = _isArray3 ? _iterator3 : _iterator3[Symbol.iterator]();;) {
      var _ref3;

      if (_isArray3) {
        if (_i3 >= _iterator3.length) break;
        _ref3 = _iterator3[_i3++];
      } else {
        _i3 = _iterator3.next();
        if (_i3.done) break;
        _ref3 = _i3.value;
      }

      var _path3 = _ref3;

      _path3.popContext();
    }

    this.queue = null;
    return stop;
  };

  _proto.visit = function visit(node, key) {
    var nodes = node[key];
    if (!nodes) return false;

    if (Array.isArray(nodes)) {
      return this.visitMultiple(nodes, node, key);
    } else {
      return this.visitSingle(node, key);
    }
  };

  return TraversalContext;
}();

exports.default = TraversalContext;