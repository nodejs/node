"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;
var _parseError = require("../parse-error");
var _default = {
  ImportMetaOutsideModule: {
    message: `import.meta may appear only with 'sourceType: "module"'`,
    code: _parseError.ParseErrorCode.SourceTypeModuleError
  },
  ImportOutsideModule: {
    message: `'import' and 'export' may appear only with 'sourceType: "module"'`,
    code: _parseError.ParseErrorCode.SourceTypeModuleError
  }
};
exports.default = _default;

//# sourceMappingURL=module-errors.js.map
