"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

var _genMapping = require("@jridgewell/gen-mapping");

class SourceMap {
  constructor(opts, code) {
    var _opts$sourceFileName;

    this._map = void 0;
    this._rawMappings = void 0;
    this._sourceFileName = void 0;
    this._lastGenLine = 0;
    this._lastSourceLine = 0;
    this._lastSourceColumn = 0;
    const map = this._map = new _genMapping.GenMapping({
      sourceRoot: opts.sourceRoot
    });
    this._sourceFileName = (_opts$sourceFileName = opts.sourceFileName) == null ? void 0 : _opts$sourceFileName.replace(/\\/g, "/");
    this._rawMappings = undefined;

    if (typeof code === "string") {
      (0, _genMapping.setSourceContent)(map, this._sourceFileName, code);
    } else if (typeof code === "object") {
      Object.keys(code).forEach(sourceFileName => {
        (0, _genMapping.setSourceContent)(map, sourceFileName.replace(/\\/g, "/"), code[sourceFileName]);
      });
    }
  }

  get() {
    return (0, _genMapping.encodedMap)(this._map);
  }

  getDecoded() {
    return (0, _genMapping.decodedMap)(this._map);
  }

  getRawMappings() {
    return this._rawMappings || (this._rawMappings = (0, _genMapping.allMappings)(this._map));
  }

  mark(generated, line, column, identifierName, filename, force) {
    const generatedLine = generated.line;
    if (this._lastGenLine !== generatedLine && line == null) return;

    if (!force && this._lastGenLine === generatedLine && this._lastSourceLine === line && this._lastSourceColumn === column) {
      return;
    }

    this._rawMappings = undefined;
    this._lastGenLine = generatedLine;
    this._lastSourceLine = line;
    this._lastSourceColumn = column;
    (0, _genMapping.addMapping)(this._map, {
      name: identifierName,
      generated,
      source: line == null ? undefined : (filename == null ? void 0 : filename.replace(/\\/g, "/")) || this._sourceFileName,
      original: line == null ? undefined : {
        line: line,
        column: column
      }
    });
  }

}

exports.default = SourceMap;