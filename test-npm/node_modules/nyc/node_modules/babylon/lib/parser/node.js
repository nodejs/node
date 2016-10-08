"use strict";

var _classCallCheck2 = require("babel-runtime/helpers/classCallCheck");

var _classCallCheck3 = _interopRequireDefault(_classCallCheck2);

var _createClass2 = require("babel-runtime/helpers/createClass");

var _createClass3 = _interopRequireDefault(_createClass2);

var _index = require("./index");

var _index2 = _interopRequireDefault(_index);

var _location = require("../util/location");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

// Start an AST node, attaching a start offset.

var pp = _index2.default.prototype;
var commentKeys = ["leadingComments", "trailingComments", "innerComments"];

var Node = function () {
  function Node(pos, loc, filename) {
    (0, _classCallCheck3.default)(this, Node);

    this.type = "";
    this.start = pos;
    this.end = 0;
    this.loc = new _location.SourceLocation(loc);
    if (filename) this.loc.filename = filename;
  }

  (0, _createClass3.default)(Node, [{
    key: "__clone",
    value: function __clone() {
      var node2 = new Node();
      for (var key in this) {
        // Do not clone comments that are already attached to the node
        if (commentKeys.indexOf(key) < 0) {
          node2[key] = this[key];
        }
      }

      return node2;
    }
  }]);
  return Node;
}();

pp.startNode = function () {
  return new Node(this.state.start, this.state.startLoc, this.filename);
};

pp.startNodeAt = function (pos, loc) {
  return new Node(pos, loc, this.filename);
};

function finishNodeAt(node, type, pos, loc) {
  node.type = type;
  node.end = pos;
  node.loc.end = loc;
  this.processComment(node);
  return node;
}

// Finish an AST node, adding `type` and `end` properties.

pp.finishNode = function (node, type) {
  return finishNodeAt.call(this, node, type, this.state.lastTokEnd, this.state.lastTokEndLoc);
};

// Finish node at given position

pp.finishNodeAt = function (node, type, pos, loc) {
  return finishNodeAt.call(this, node, type, pos, loc);
};