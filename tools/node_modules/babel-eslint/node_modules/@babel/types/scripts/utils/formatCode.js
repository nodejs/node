"use strict";
const prettier = require("prettier");

module.exports = function formatCode(code, filename) {
  filename = filename || __filename;
  const prettierConfig = prettier.resolveConfig.sync(filename);
  prettierConfig.filepath = filename;
  prettierConfig.parser = "babylon";

  return prettier.format(code, prettierConfig);
};
