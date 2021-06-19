const babel = require("./babel-core.cjs");

const convert = require("../convert/index.cjs");

const {
  getVisitorKeys,
  getTokLabels
} = require("./ast-info.cjs");

const extractParserOptionsPlugin = require("./extract-parser-options-plugin.cjs");

const ref = {};
let extractParserOptionsConfigItem;
const MULTIPLE_OVERRIDES = /More than one plugin attempted to override parsing/;

module.exports = function maybeParse(code, options) {
  if (!extractParserOptionsConfigItem) {
    extractParserOptionsConfigItem = babel.createConfigItem([extractParserOptionsPlugin, ref], {
      dirname: __dirname,
      type: "plugin"
    });
  }

  options.plugins.push(extractParserOptionsConfigItem);

  try {
    return {
      parserOptions: babel.parseSync(code, options),
      ast: null
    };
  } catch (err) {
    if (!MULTIPLE_OVERRIDES.test(err.message)) {
      throw err;
    }
  }

  let ast;

  try {
    ast = babel.parseSync(code, options);
  } catch (err) {
    throw convert.error(err);
  }

  return {
    ast: convert.ast(ast, code, getTokLabels(), getVisitorKeys()),
    parserOptions: null
  };
};