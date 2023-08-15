"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.defaultOptions = void 0;
exports.getOptions = getOptions;
const defaultOptions = {
  sourceType: "script",
  sourceFilename: undefined,
  startColumn: 0,
  startLine: 1,
  allowAwaitOutsideFunction: false,
  allowReturnOutsideFunction: false,
  allowNewTargetOutsideFunction: false,
  allowImportExportEverywhere: false,
  allowSuperOutsideMethod: false,
  allowUndeclaredExports: false,
  plugins: [],
  strictMode: null,
  ranges: false,
  tokens: false,
  createParenthesizedExpressions: false,
  errorRecovery: false,
  attachComment: true,
  annexB: true
};
exports.defaultOptions = defaultOptions;
function getOptions(opts) {
  if (opts == null) {
    return Object.assign({}, defaultOptions);
  }
  if (opts.annexB != null && opts.annexB !== false) {
    throw new Error("The `annexB` option can only be set to `false`.");
  }
  const options = {};
  for (const key of Object.keys(defaultOptions)) {
    var _opts$key;
    options[key] = (_opts$key = opts[key]) != null ? _opts$key : defaultOptions[key];
  }
  return options;
}

//# sourceMappingURL=options.js.map
