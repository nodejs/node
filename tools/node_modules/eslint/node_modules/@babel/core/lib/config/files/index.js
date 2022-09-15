"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
Object.defineProperty(exports, "ROOT_CONFIG_FILENAMES", {
  enumerable: true,
  get: function () {
    return _configuration.ROOT_CONFIG_FILENAMES;
  }
});
Object.defineProperty(exports, "findConfigUpwards", {
  enumerable: true,
  get: function () {
    return _configuration.findConfigUpwards;
  }
});
Object.defineProperty(exports, "findPackageData", {
  enumerable: true,
  get: function () {
    return _package.findPackageData;
  }
});
Object.defineProperty(exports, "findRelativeConfig", {
  enumerable: true,
  get: function () {
    return _configuration.findRelativeConfig;
  }
});
Object.defineProperty(exports, "findRootConfig", {
  enumerable: true,
  get: function () {
    return _configuration.findRootConfig;
  }
});
Object.defineProperty(exports, "loadConfig", {
  enumerable: true,
  get: function () {
    return _configuration.loadConfig;
  }
});
Object.defineProperty(exports, "loadPlugin", {
  enumerable: true,
  get: function () {
    return plugins.loadPlugin;
  }
});
Object.defineProperty(exports, "loadPreset", {
  enumerable: true,
  get: function () {
    return plugins.loadPreset;
  }
});
exports.resolvePreset = exports.resolvePlugin = void 0;
Object.defineProperty(exports, "resolveShowConfigPath", {
  enumerable: true,
  get: function () {
    return _configuration.resolveShowConfigPath;
  }
});

var _package = require("./package");

var _configuration = require("./configuration");

var plugins = require("./plugins");

function _gensync() {
  const data = require("gensync");

  _gensync = function () {
    return data;
  };

  return data;
}

({});

const resolvePlugin = _gensync()(plugins.resolvePlugin).sync;

exports.resolvePlugin = resolvePlugin;

const resolvePreset = _gensync()(plugins.resolvePreset).sync;

exports.resolvePreset = resolvePreset;
0 && 0;

//# sourceMappingURL=index.js.map
