const semver = require("semver");

const {
  normalizeESLintConfig
} = require("./configuration.cjs");

const analyzeScope = require("./analyze-scope.cjs");

const {
  getVersion,
  getVisitorKeys,
  getTokLabels,
  maybeParse
} = require("./client.cjs");

const convert = require("./convert/index.cjs");

const babelParser = require((((v, w) => (v = v.split("."), w = w.split("."), +v[0] > +w[0] || v[0] == w[0] && +v[1] >= +w[1]))(process.versions.node, "8.9") ? require.resolve : (r, {
  paths: [b]
}, M = require("module")) => {
  let f = M._findPath(r, M._nodeModulePaths(b).concat(b));

  if (f) return f;
  f = new Error(`Cannot resolve module '${r}'`);
  f.code = "MODULE_NOT_FOUND";
  throw f;
})("@babel/parser", {
  paths: [require.resolve("@babel/core/package.json")]
}));

let isRunningMinSupportedCoreVersion = null;

function baseParse(code, options) {
  const minSupportedCoreVersion = ">=7.2.0";

  if (typeof isRunningMinSupportedCoreVersion !== "boolean") {
    isRunningMinSupportedCoreVersion = semver.satisfies(getVersion(), minSupportedCoreVersion);
  }

  if (!isRunningMinSupportedCoreVersion) {
    throw new Error(`@babel/eslint-parser@${"7.14.5"} does not support @babel/core@${getVersion()}. Please upgrade to @babel/core@${minSupportedCoreVersion}.`);
  }

  const {
    ast,
    parserOptions
  } = maybeParse(code, options);
  if (ast) return ast;

  try {
    return convert.ast(babelParser.parse(code, parserOptions), code, getTokLabels(), getVisitorKeys());
  } catch (err) {
    throw convert.error(err);
  }
}

exports.parse = function (code, options = {}) {
  return baseParse(code, normalizeESLintConfig(options));
};

exports.parseForESLint = function (code, options = {}) {
  const normalizedOptions = normalizeESLintConfig(options);
  const ast = baseParse(code, normalizedOptions);
  const scopeManager = analyzeScope(ast, normalizedOptions);
  return {
    ast,
    scopeManager,
    visitorKeys: getVisitorKeys()
  };
};