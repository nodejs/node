"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.loadPlugin = loadPlugin;
exports.loadPreset = loadPreset;
exports.resolvePlugin = resolvePlugin;
exports.resolvePreset = resolvePreset;
function _debug() {
  const data = require("debug");
  _debug = function () {
    return data;
  };
  return data;
}
function _path() {
  const data = require("path");
  _path = function () {
    return data;
  };
  return data;
}
function _gensync() {
  const data = require("gensync");
  _gensync = function () {
    return data;
  };
  return data;
}
var _async = require("../../gensync-utils/async");
var _moduleTypes = require("./module-types");
function _url() {
  const data = require("url");
  _url = function () {
    return data;
  };
  return data;
}
var _importMetaResolve = require("./import-meta-resolve");
function asyncGeneratorStep(gen, resolve, reject, _next, _throw, key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { Promise.resolve(value).then(_next, _throw); } }
function _asyncToGenerator(fn) { return function () { var self = this, args = arguments; return new Promise(function (resolve, reject) { var gen = fn.apply(self, args); function _next(value) { asyncGeneratorStep(gen, resolve, reject, _next, _throw, "next", value); } function _throw(err) { asyncGeneratorStep(gen, resolve, reject, _next, _throw, "throw", err); } _next(undefined); }); }; }
const debug = _debug()("babel:config:loading:files:plugins");
const EXACT_RE = /^module:/;
const BABEL_PLUGIN_PREFIX_RE = /^(?!@|module:|[^/]+\/|babel-plugin-)/;
const BABEL_PRESET_PREFIX_RE = /^(?!@|module:|[^/]+\/|babel-preset-)/;
const BABEL_PLUGIN_ORG_RE = /^(@babel\/)(?!plugin-|[^/]+\/)/;
const BABEL_PRESET_ORG_RE = /^(@babel\/)(?!preset-|[^/]+\/)/;
const OTHER_PLUGIN_ORG_RE = /^(@(?!babel\/)[^/]+\/)(?![^/]*babel-plugin(?:-|\/|$)|[^/]+\/)/;
const OTHER_PRESET_ORG_RE = /^(@(?!babel\/)[^/]+\/)(?![^/]*babel-preset(?:-|\/|$)|[^/]+\/)/;
const OTHER_ORG_DEFAULT_RE = /^(@(?!babel$)[^/]+)$/;
function* resolvePlugin(name, dirname) {
  return yield* resolveStandardizedName("plugin", name, dirname);
}
function* resolvePreset(name, dirname) {
  return yield* resolveStandardizedName("preset", name, dirname);
}
function* loadPlugin(name, dirname) {
  const filepath = yield* resolvePlugin(name, dirname);
  const value = yield* requireModule("plugin", filepath);
  debug("Loaded plugin %o from %o.", name, dirname);
  return {
    filepath,
    value
  };
}
function* loadPreset(name, dirname) {
  const filepath = yield* resolvePreset(name, dirname);
  const value = yield* requireModule("preset", filepath);
  debug("Loaded preset %o from %o.", name, dirname);
  return {
    filepath,
    value
  };
}
function standardizeName(type, name) {
  if (_path().isAbsolute(name)) return name;
  const isPreset = type === "preset";
  return name.replace(isPreset ? BABEL_PRESET_PREFIX_RE : BABEL_PLUGIN_PREFIX_RE, `babel-${type}-`).replace(isPreset ? BABEL_PRESET_ORG_RE : BABEL_PLUGIN_ORG_RE, `$1${type}-`).replace(isPreset ? OTHER_PRESET_ORG_RE : OTHER_PLUGIN_ORG_RE, `$1babel-${type}-`).replace(OTHER_ORG_DEFAULT_RE, `$1/babel-${type}`).replace(EXACT_RE, "");
}
function* resolveAlternativesHelper(type, name) {
  const standardizedName = standardizeName(type, name);
  const {
    error,
    value
  } = yield standardizedName;
  if (!error) return value;
  if (error.code !== "MODULE_NOT_FOUND") throw error;
  if (standardizedName !== name && !(yield name).error) {
    error.message += `\n- If you want to resolve "${name}", use "module:${name}"`;
  }
  if (!(yield standardizeName(type, "@babel/" + name)).error) {
    error.message += `\n- Did you mean "@babel/${name}"?`;
  }
  const oppositeType = type === "preset" ? "plugin" : "preset";
  if (!(yield standardizeName(oppositeType, name)).error) {
    error.message += `\n- Did you accidentally pass a ${oppositeType} as a ${type}?`;
  }
  throw error;
}
function tryRequireResolve(id, {
  paths: [dirname]
}) {
  try {
    return {
      error: null,
      value: (((v, w) => (v = v.split("."), w = w.split("."), +v[0] > +w[0] || v[0] == w[0] && +v[1] >= +w[1]))(process.versions.node, "8.9") ? require.resolve : (r, {
        paths: [b]
      }, M = require("module")) => {
        let f = M._findPath(r, M._nodeModulePaths(b).concat(b));
        if (f) return f;
        f = new Error(`Cannot resolve module '${r}'`);
        f.code = "MODULE_NOT_FOUND";
        throw f;
      })(id, {
        paths: [dirname]
      })
    };
  } catch (error) {
    return {
      error,
      value: null
    };
  }
}
function tryImportMetaResolve(_x, _x2) {
  return _tryImportMetaResolve.apply(this, arguments);
}
function _tryImportMetaResolve() {
  _tryImportMetaResolve = _asyncToGenerator(function* (id, options) {
    try {
      return {
        error: null,
        value: yield (0, _importMetaResolve.default)(id, options)
      };
    } catch (error) {
      return {
        error,
        value: null
      };
    }
  });
  return _tryImportMetaResolve.apply(this, arguments);
}
function resolveStandardizedNameForRequire(type, name, dirname) {
  const it = resolveAlternativesHelper(type, name);
  let res = it.next();
  while (!res.done) {
    res = it.next(tryRequireResolve(res.value, {
      paths: [dirname]
    }));
  }
  return res.value;
}
function resolveStandardizedNameForImport(_x3, _x4, _x5) {
  return _resolveStandardizedNameForImport.apply(this, arguments);
}
function _resolveStandardizedNameForImport() {
  _resolveStandardizedNameForImport = _asyncToGenerator(function* (type, name, dirname) {
    const parentUrl = (0, _url().pathToFileURL)(_path().join(dirname, "./babel-virtual-resolve-base.js")).href;
    const it = resolveAlternativesHelper(type, name);
    let res = it.next();
    while (!res.done) {
      res = it.next(yield tryImportMetaResolve(res.value, parentUrl));
    }
    return (0, _url().fileURLToPath)(res.value);
  });
  return _resolveStandardizedNameForImport.apply(this, arguments);
}
const resolveStandardizedName = _gensync()({
  sync(type, name, dirname = process.cwd()) {
    return resolveStandardizedNameForRequire(type, name, dirname);
  },
  async(type, name, dirname = process.cwd()) {
    return _asyncToGenerator(function* () {
      if (!_moduleTypes.supportsESM) {
        return resolveStandardizedNameForRequire(type, name, dirname);
      }
      try {
        return yield resolveStandardizedNameForImport(type, name, dirname);
      } catch (e) {
        try {
          return resolveStandardizedNameForRequire(type, name, dirname);
        } catch (e2) {
          if (e.type === "MODULE_NOT_FOUND") throw e;
          if (e2.type === "MODULE_NOT_FOUND") throw e2;
          throw e;
        }
      }
    })();
  }
});
{
  var LOADING_MODULES = new Set();
}
function* requireModule(type, name) {
  {
    if (!(yield* (0, _async.isAsync)()) && LOADING_MODULES.has(name)) {
      throw new Error(`Reentrant ${type} detected trying to load "${name}". This module is not ignored ` + "and is trying to load itself while compiling itself, leading to a dependency cycle. " + 'We recommend adding it to your "ignore" list in your babelrc, or to a .babelignore.');
    }
  }
  try {
    {
      LOADING_MODULES.add(name);
    }
    return yield* (0, _moduleTypes.default)(name, `You appear to be using a native ECMAScript module ${type}, ` + "which is only supported when running Babel asynchronously.", true);
  } catch (err) {
    err.message = `[BABEL]: ${err.message} (While processing: ${name})`;
    throw err;
  } finally {
    {
      LOADING_MODULES.delete(name);
    }
  }
}
0 && 0;

//# sourceMappingURL=plugins.js.map
