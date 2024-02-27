"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.meta = void 0;
exports.parseForESLint = parseForESLint;
var _client = require("./client.cjs");
const [major, minor] = process.versions.node.split(".").map(Number);
if (major < 12 || major === 12 && minor < 3) {
  throw new Error("@babel/eslint-parser/experimental-worker requires Node.js >= 12.3.0");
}
const normalizeESLintConfig = require("./configuration.cjs");
const analyzeScope = require("./analyze-scope.cjs");
const baseParse = require("./parse.cjs");
const client = new _client.WorkerClient();
const meta = exports.meta = {
  name: "@babel/eslint-parser/experimental-worker",
  version: "7.23.10"
};
function parseForESLint(code, options = {}) {
  const normalizedOptions = normalizeESLintConfig(options);
  const ast = baseParse(code, normalizedOptions, client);
  const scopeManager = analyzeScope(ast, normalizedOptions, client);
  return {
    ast,
    scopeManager,
    visitorKeys: client.getVisitorKeys()
  };
}

//# sourceMappingURL=experimental-worker.cjs.map
