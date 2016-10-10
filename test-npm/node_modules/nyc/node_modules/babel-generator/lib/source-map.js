"use strict";

exports.__esModule = true;

var _keys = require("babel-runtime/core-js/object/keys");

var _keys2 = _interopRequireDefault(_keys);

var _typeof2 = require("babel-runtime/helpers/typeof");

var _typeof3 = _interopRequireDefault(_typeof2);

var _classCallCheck2 = require("babel-runtime/helpers/classCallCheck");

var _classCallCheck3 = _interopRequireDefault(_classCallCheck2);

var _sourceMap = require("source-map");

var _sourceMap2 = _interopRequireDefault(_sourceMap);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/**
 * Build a sourcemap.
 */

var SourceMap = function () {
  function SourceMap(opts, code) {
    var _this = this;

    (0, _classCallCheck3.default)(this, SourceMap);

    this._opts = opts;
    this._map = new _sourceMap2.default.SourceMapGenerator({
      file: opts.sourceMapTarget,
      sourceRoot: opts.sourceRoot
    });

    if (typeof code === "string") {
      this._map.setSourceContent(opts.sourceFileName, code);
    } else if ((typeof code === "undefined" ? "undefined" : (0, _typeof3.default)(code)) === "object") {
      (0, _keys2.default)(code).forEach(function (sourceFileName) {
        _this._map.setSourceContent(sourceFileName, code[sourceFileName]);
      });
    }
  }

  /**
   * Get the sourcemap.
   */

  SourceMap.prototype.get = function get() {
    return this._map.toJSON();
  };

  /**
   * Mark the current generated position with a source position. May also be passed null line/column
   * values to insert a mapping to nothing.
   */

  SourceMap.prototype.mark = function mark(generatedLine, generatedColumn, line, column, filename) {
    // Adding an empty mapping at the start of a generated line just clutters the map.
    if (this._lastGenLine !== generatedLine && line === null) return;

    // If this mapping points to the same source location as the last one, we can ignore it since
    // the previous one covers it.
    if (this._lastGenLine === generatedLine && this._lastSourceLine === line && this._lastSourceColumn === column) {
      return;
    }

    this._lastGenLine = generatedLine;
    this._lastSourceLine = line;
    this._lastSourceColumn = column;

    this._map.addMapping({
      generated: {
        line: generatedLine,
        column: generatedColumn
      },
      source: line == null ? null : filename || this._opts.sourceFileName,
      original: line == null ? null : {
        line: line,
        column: column
      }
    });
  };

  return SourceMap;
}();

exports.default = SourceMap;
module.exports = exports["default"];