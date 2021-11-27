"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.resolveBrowserslistConfigFile = resolveBrowserslistConfigFile;
exports.resolveTargets = resolveTargets;

function _helperCompilationTargets() {
  const data = require("@babel/helper-compilation-targets");

  _helperCompilationTargets = function () {
    return data;
  };

  return data;
}

function resolveBrowserslistConfigFile(browserslistConfigFile, configFilePath) {
  return undefined;
}

function resolveTargets(options, root) {
  let targets = options.targets;

  if (typeof targets === "string" || Array.isArray(targets)) {
    targets = {
      browsers: targets
    };
  }

  if (targets && targets.esmodules) {
    targets = Object.assign({}, targets, {
      esmodules: "intersect"
    });
  }

  return (0, _helperCompilationTargets().default)(targets, {
    ignoreBrowserslistConfig: true,
    browserslistEnv: options.browserslistEnv
  });
}