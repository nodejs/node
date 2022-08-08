"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = loadCjsOrMjsDefault;
exports.supportsESM = void 0;

var _async = require("../../gensync-utils/async");

function _path() {
  const data = require("path");

  _path = function () {
    return data;
  };

  return data;
}

function _url() {
  const data = require("url");

  _url = function () {
    return data;
  };

  return data;
}

function _module() {
  const data = require("module");

  _module = function () {
    return data;
  };

  return data;
}

function _semver() {
  const data = require("semver");

  _semver = function () {
    return data;
  };

  return data;
}

function asyncGeneratorStep(gen, resolve, reject, _next, _throw, key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { Promise.resolve(value).then(_next, _throw); } }

function _asyncToGenerator(fn) { return function () { var self = this, args = arguments; return new Promise(function (resolve, reject) { var gen = fn.apply(self, args); function _next(value) { asyncGeneratorStep(gen, resolve, reject, _next, _throw, "next", value); } function _throw(err) { asyncGeneratorStep(gen, resolve, reject, _next, _throw, "throw", err); } _next(undefined); }); }; }

let import_;

try {
  import_ = require("./import.cjs");
} catch (_unused) {}

const supportsESM = _semver().satisfies(process.versions.node, "^12.17 || >=13.2");

exports.supportsESM = supportsESM;

function* loadCjsOrMjsDefault(filepath, asyncError, fallbackToTranspiledModule = false) {
  switch (guessJSModuleType(filepath)) {
    case "cjs":
      return loadCjsDefault(filepath, fallbackToTranspiledModule);

    case "unknown":
      try {
        return loadCjsDefault(filepath, fallbackToTranspiledModule);
      } catch (e) {
        if (e.code !== "ERR_REQUIRE_ESM") throw e;
      }

    case "mjs":
      if (yield* (0, _async.isAsync)()) {
        return yield* (0, _async.waitFor)(loadMjsDefault(filepath));
      }

      throw new Error(asyncError);
  }
}

function guessJSModuleType(filename) {
  switch (_path().extname(filename)) {
    case ".cjs":
      return "cjs";

    case ".mjs":
      return "mjs";

    default:
      return "unknown";
  }
}

function loadCjsDefault(filepath, fallbackToTranspiledModule) {
  const module = require(filepath);

  return module != null && module.__esModule ? module.default || (fallbackToTranspiledModule ? module : undefined) : module;
}

function loadMjsDefault(_x) {
  return _loadMjsDefault.apply(this, arguments);
}

function _loadMjsDefault() {
  _loadMjsDefault = _asyncToGenerator(function* (filepath) {
    if (!import_) {
      throw new Error("Internal error: Native ECMAScript modules aren't supported" + " by this platform.\n");
    }

    const module = yield import_((0, _url().pathToFileURL)(filepath));
    return module.default;
  });
  return _loadMjsDefault.apply(this, arguments);
}

0 && 0;