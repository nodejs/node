"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

var _sourceMap = require("source-map");

class SourceMap {
  constructor(opts, code) {
    this._cachedMap = void 0;
    this._code = void 0;
    this._opts = void 0;
    this._rawMappings = void 0;
    this._lastGenLine = void 0;
    this._lastSourceLine = void 0;
    this._lastSourceColumn = void 0;
    this._cachedMap = null;
    this._code = code;
    this._opts = opts;
    this._rawMappings = [];
  }

  get() {
    if (!this._cachedMap) {
      const map = this._cachedMap = new _sourceMap.SourceMapGenerator({
        sourceRoot: this._opts.sourceRoot
      });
      const code = this._code;

      if (typeof code === "string") {
        map.setSourceContent(this._opts.sourceFileName.replace(/\\/g, "/"), code);
      } else if (typeof code === "object") {
        Object.keys(code).forEach(sourceFileName => {
          map.setSourceContent(sourceFileName.replace(/\\/g, "/"), code[sourceFileName]);
        });
      }

      this._rawMappings.forEach(mapping => map.addMapping(mapping), map);
    }

    return this._cachedMap.toJSON();
  }

  getRawMappings() {
    return this._rawMappings.slice();
  }

  mark(generatedLine, generatedColumn, line, column, identifierName, filename, force) {
    if (this._lastGenLine !== generatedLine && line === null) return;

    if (!force && this._lastGenLine === generatedLine && this._lastSourceLine === line && this._lastSourceColumn === column) {
      return;
    }

    this._cachedMap = null;
    this._lastGenLine = generatedLine;
    this._lastSourceLine = line;
    this._lastSourceColumn = column;

    this._rawMappings.push({
      name: identifierName || undefined,
      generated: {
        line: generatedLine,
        column: generatedColumn
      },
      source: line == null ? undefined : (filename || this._opts.sourceFileName).replace(/\\/g, "/"),
      original: line == null ? undefined : {
        line: line,
        column: column
      }
    });
  }

}

exports.default = SourceMap;