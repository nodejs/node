"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.plugins = undefined;

var _getIterator2 = require("babel-runtime/core-js/get-iterator");

var _getIterator3 = _interopRequireDefault(_getIterator2);

var _getPrototypeOf = require("babel-runtime/core-js/object/get-prototype-of");

var _getPrototypeOf2 = _interopRequireDefault(_getPrototypeOf);

var _classCallCheck2 = require("babel-runtime/helpers/classCallCheck");

var _classCallCheck3 = _interopRequireDefault(_classCallCheck2);

var _createClass2 = require("babel-runtime/helpers/createClass");

var _createClass3 = _interopRequireDefault(_createClass2);

var _possibleConstructorReturn2 = require("babel-runtime/helpers/possibleConstructorReturn");

var _possibleConstructorReturn3 = _interopRequireDefault(_possibleConstructorReturn2);

var _inherits2 = require("babel-runtime/helpers/inherits");

var _inherits3 = _interopRequireDefault(_inherits2);

var _identifier = require("../util/identifier");

var _options = require("../options");

var _tokenizer = require("../tokenizer");

var _tokenizer2 = _interopRequireDefault(_tokenizer);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

var plugins = exports.plugins = {};

var Parser = function (_Tokenizer) {
  (0, _inherits3.default)(Parser, _Tokenizer);

  function Parser(options, input) {
    (0, _classCallCheck3.default)(this, Parser);

    options = (0, _options.getOptions)(options);

    var _this = (0, _possibleConstructorReturn3.default)(this, (0, _getPrototypeOf2.default)(Parser).call(this, options, input));

    _this.options = options;
    _this.inModule = _this.options.sourceType === "module";
    _this.isReservedWord = _identifier.reservedWords[6];
    _this.input = input;
    _this.plugins = _this.loadPlugins(_this.options.plugins);
    _this.filename = options.sourceFilename;

    // If enabled, skip leading hashbang line.
    if (_this.state.pos === 0 && _this.input[0] === "#" && _this.input[1] === "!") {
      _this.skipLineComment(2);
    }
    return _this;
  }

  (0, _createClass3.default)(Parser, [{
    key: "hasPlugin",
    value: function hasPlugin(name) {
      return !!(this.plugins["*"] || this.plugins[name]);
    }
  }, {
    key: "extend",
    value: function extend(name, f) {
      this[name] = f(this[name]);
    }
  }, {
    key: "loadPlugins",
    value: function loadPlugins(plugins) {
      var pluginMap = {};

      if (plugins.indexOf("flow") >= 0) {
        // ensure flow plugin loads last
        plugins = plugins.filter(function (plugin) {
          return plugin !== "flow";
        });
        plugins.push("flow");
      }

      var _iteratorNormalCompletion = true;
      var _didIteratorError = false;
      var _iteratorError = undefined;

      try {
        for (var _iterator = (0, _getIterator3.default)(plugins), _step; !(_iteratorNormalCompletion = (_step = _iterator.next()).done); _iteratorNormalCompletion = true) {
          var name = _step.value;

          if (!pluginMap[name]) {
            pluginMap[name] = true;

            var plugin = exports.plugins[name];
            if (plugin) plugin(this);
          }
        }
      } catch (err) {
        _didIteratorError = true;
        _iteratorError = err;
      } finally {
        try {
          if (!_iteratorNormalCompletion && _iterator.return) {
            _iterator.return();
          }
        } finally {
          if (_didIteratorError) {
            throw _iteratorError;
          }
        }
      }

      return pluginMap;
    }
  }, {
    key: "parse",
    value: function parse() {
      var file = this.startNode();
      var program = this.startNode();
      this.nextToken();
      return this.parseTopLevel(file, program);
    }
  }]);
  return Parser;
}(_tokenizer2.default);

exports.default = Parser;