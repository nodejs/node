"use strict";

const babelP = import("./lib/index.js");

const functionNames = [
  "createConfigItem",
  "loadPartialConfig",
  "loadOptions",
  "transform",
  "transformFile",
  "transformFromAst",
  "parse",
];

for (const name of functionNames) {
  exports[`${name}Sync`] = function () {
    throw new Error(
      `"${name}Sync" is not supported when loading @babel/core using require()`
    );
  };
  exports[name] = function (...args) {
    babelP.then(babel => {
      babel[name](...args);
    });
  };
  exports[`${name}Async`] = function (...args) {
    return babelP.then(babel => babel[`${name}Async`](...args));
  };
}
