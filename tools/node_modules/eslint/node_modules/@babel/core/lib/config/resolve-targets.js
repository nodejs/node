"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.resolveBrowserslistConfigFile = resolveBrowserslistConfigFile;
exports.resolveTargets = resolveTargets;

function _path() {
  const data = require("path");

  _path = function () {
    return data;
  };

  return data;
}

function _helperCompilationTargets() {
  const data = require("@babel/helper-compilation-targets");

  _helperCompilationTargets = function () {
    return data;
  };

  return data;
}

({});

function resolveBrowserslistConfigFile(browserslistConfigFile, configFileDir) {
  return _path().resolve(configFileDir, browserslistConfigFile);
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

  const {
    browserslistConfigFile
  } = options;
  let configFile;
  let ignoreBrowserslistConfig = false;

  if (typeof browserslistConfigFile === "string") {
    configFile = browserslistConfigFile;
  } else {
    ignoreBrowserslistConfig = browserslistConfigFile === false;
  }

  return (0, _helperCompilationTargets().default)(targets, {
    ignoreBrowserslistConfig,
    configFile,
    configPath: root,
    browserslistEnv: options.browserslistEnv
  });
}