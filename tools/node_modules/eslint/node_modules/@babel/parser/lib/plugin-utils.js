"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getPluginOption = getPluginOption;
exports.hasPlugin = hasPlugin;
exports.mixinPlugins = exports.mixinPluginNames = void 0;
exports.validatePlugins = validatePlugins;
var _estree = require("./plugins/estree");
var _flow = require("./plugins/flow");
var _jsx = require("./plugins/jsx");
var _typescript = require("./plugins/typescript");
var _placeholders = require("./plugins/placeholders");
var _v8intrinsic = require("./plugins/v8intrinsic");
function hasPlugin(plugins, expectedConfig) {
  const [expectedName, expectedOptions] = typeof expectedConfig === "string" ? [expectedConfig, {}] : expectedConfig;
  const expectedKeys = Object.keys(expectedOptions);
  const expectedOptionsIsEmpty = expectedKeys.length === 0;
  return plugins.some(p => {
    if (typeof p === "string") {
      return expectedOptionsIsEmpty && p === expectedName;
    } else {
      const [pluginName, pluginOptions] = p;
      if (pluginName !== expectedName) {
        return false;
      }
      for (const key of expectedKeys) {
        if (pluginOptions[key] !== expectedOptions[key]) {
          return false;
        }
      }
      return true;
    }
  });
}
function getPluginOption(plugins, name, option) {
  const plugin = plugins.find(plugin => {
    if (Array.isArray(plugin)) {
      return plugin[0] === name;
    } else {
      return plugin === name;
    }
  });
  if (plugin && Array.isArray(plugin) && plugin.length > 1) {
    return plugin[1][option];
  }
  return null;
}
const PIPELINE_PROPOSALS = ["minimal", "fsharp", "hack", "smart"];
const TOPIC_TOKENS = ["^^", "@@", "^", "%", "#"];
const RECORD_AND_TUPLE_SYNTAX_TYPES = ["hash", "bar"];
function validatePlugins(plugins) {
  if (hasPlugin(plugins, "decorators")) {
    if (hasPlugin(plugins, "decorators-legacy")) {
      throw new Error("Cannot use the decorators and decorators-legacy plugin together");
    }
    const decoratorsBeforeExport = getPluginOption(plugins, "decorators", "decoratorsBeforeExport");
    if (decoratorsBeforeExport != null && typeof decoratorsBeforeExport !== "boolean") {
      throw new Error("'decoratorsBeforeExport' must be a boolean, if specified.");
    }
    const allowCallParenthesized = getPluginOption(plugins, "decorators", "allowCallParenthesized");
    if (allowCallParenthesized != null && typeof allowCallParenthesized !== "boolean") {
      throw new Error("'allowCallParenthesized' must be a boolean.");
    }
  }
  if (hasPlugin(plugins, "flow") && hasPlugin(plugins, "typescript")) {
    throw new Error("Cannot combine flow and typescript plugins.");
  }
  if (hasPlugin(plugins, "placeholders") && hasPlugin(plugins, "v8intrinsic")) {
    throw new Error("Cannot combine placeholders and v8intrinsic plugins.");
  }
  if (hasPlugin(plugins, "pipelineOperator")) {
    const proposal = getPluginOption(plugins, "pipelineOperator", "proposal");
    if (!PIPELINE_PROPOSALS.includes(proposal)) {
      const proposalList = PIPELINE_PROPOSALS.map(p => `"${p}"`).join(", ");
      throw new Error(`"pipelineOperator" requires "proposal" option whose value must be one of: ${proposalList}.`);
    }
    const tupleSyntaxIsHash = hasPlugin(plugins, ["recordAndTuple", {
      syntaxType: "hash"
    }]);
    if (proposal === "hack") {
      if (hasPlugin(plugins, "placeholders")) {
        throw new Error("Cannot combine placeholders plugin and Hack-style pipes.");
      }
      if (hasPlugin(plugins, "v8intrinsic")) {
        throw new Error("Cannot combine v8intrinsic plugin and Hack-style pipes.");
      }
      const topicToken = getPluginOption(plugins, "pipelineOperator", "topicToken");
      if (!TOPIC_TOKENS.includes(topicToken)) {
        const tokenList = TOPIC_TOKENS.map(t => `"${t}"`).join(", ");
        throw new Error(`"pipelineOperator" in "proposal": "hack" mode also requires a "topicToken" option whose value must be one of: ${tokenList}.`);
      }
      if (topicToken === "#" && tupleSyntaxIsHash) {
        throw new Error('Plugin conflict between `["pipelineOperator", { proposal: "hack", topicToken: "#" }]` and `["recordAndtuple", { syntaxType: "hash"}]`.');
      }
    } else if (proposal === "smart" && tupleSyntaxIsHash) {
      throw new Error('Plugin conflict between `["pipelineOperator", { proposal: "smart" }]` and `["recordAndtuple", { syntaxType: "hash"}]`.');
    }
  }
  if (hasPlugin(plugins, "moduleAttributes")) {
    {
      if (hasPlugin(plugins, "importAssertions") || hasPlugin(plugins, "importAttributes")) {
        throw new Error("Cannot combine importAssertions, importAttributes and moduleAttributes plugins.");
      }
      const moduleAttributesVersionPluginOption = getPluginOption(plugins, "moduleAttributes", "version");
      if (moduleAttributesVersionPluginOption !== "may-2020") {
        throw new Error("The 'moduleAttributes' plugin requires a 'version' option," + " representing the last proposal update. Currently, the" + " only supported value is 'may-2020'.");
      }
    }
  }
  if (hasPlugin(plugins, "importAssertions") && hasPlugin(plugins, "importAttributes")) {
    throw new Error("Cannot combine importAssertions and importAttributes plugins.");
  }
  if (hasPlugin(plugins, "recordAndTuple") && getPluginOption(plugins, "recordAndTuple", "syntaxType") != null && !RECORD_AND_TUPLE_SYNTAX_TYPES.includes(getPluginOption(plugins, "recordAndTuple", "syntaxType"))) {
    throw new Error("The 'syntaxType' option of the 'recordAndTuple' plugin must be one of: " + RECORD_AND_TUPLE_SYNTAX_TYPES.map(p => `'${p}'`).join(", "));
  }
  if (hasPlugin(plugins, "asyncDoExpressions") && !hasPlugin(plugins, "doExpressions")) {
    const error = new Error("'asyncDoExpressions' requires 'doExpressions', please add 'doExpressions' to parser plugins.");
    error.missingPlugins = "doExpressions";
    throw error;
  }
}
const mixinPlugins = {
  estree: _estree.default,
  jsx: _jsx.default,
  flow: _flow.default,
  typescript: _typescript.default,
  v8intrinsic: _v8intrinsic.default,
  placeholders: _placeholders.default
};
exports.mixinPlugins = mixinPlugins;
const mixinPluginNames = Object.keys(mixinPlugins);
exports.mixinPluginNames = mixinPluginNames;

//# sourceMappingURL=plugin-utils.js.map
