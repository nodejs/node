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
    return (0, _genMapping.toEncodedMap)(this._map);
  }
  getDecoded() {
    return (0, _genMapping.toDecodedMap)(this._map);
  }
  getRawMappings() {
    return this._rawMappings || (this._rawMappings = (0, _genMapping.allMappings)(this._map));
  }

  mark(generated, line, column, identifierName, filename) {
    this._rawMappings = undefined;
    (0, _genMapping.maybeAddMapping)(this._map, {
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

//# sourceMappingURL=source-map.js.map
