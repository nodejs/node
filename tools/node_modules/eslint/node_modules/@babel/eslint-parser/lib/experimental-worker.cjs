const [major, minor] = process.versions.node.split(".").map(Number);
if (major < 12 || major === 12 && minor < 3) {
  throw new Error("@babel/eslint-parser/experimental-worker requires Node.js >= 12.3.0");
}
const {
  normalizeESLintConfig
} = require("./configuration.cjs");
const analyzeScope = require("./analyze-scope.cjs");
const baseParse = require("./parse.cjs");
const {
  WorkerClient
} = require("./client.cjs");
const client = new WorkerClient();
exports.meta = {
  name: "@babel/eslint-parser/experimental-worker",
  version: "7.22.9"
};
exports.parseForESLint = function (code, options = {}) {
  const normalizedOptions = normalizeESLintConfig(options);
  const ast = baseParse(code, normalizedOptions, client);
  const scopeManager = analyzeScope(ast, normalizedOptions, client);
  return {
    ast,
    scopeManager,
    visitorKeys: client.getVisitorKeys()
  };
};

//# sourceMappingURL=experimental-worker.cjs.map
