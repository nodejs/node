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

var _moduleTypes = require("./module-types");

function _module() {
  const data = require("module");

  _module = function () {
    return data;
  };

  return data;
}

const debug = _debug()("babel:config:loading:files:plugins");

const EXACT_RE = /^module:/;
const BABEL_PLUGIN_PREFIX_RE = /^(?!@|module:|[^/]+\/|babel-plugin-)/;
const BABEL_PRESET_PREFIX_RE = /^(?!@|module:|[^/]+\/|babel-preset-)/;
const BABEL_PLUGIN_ORG_RE = /^(@babel\/)(?!plugin-|[^/]+\/)/;
const BABEL_PRESET_ORG_RE = /^(@babel\/)(?!preset-|[^/]+\/)/;
const OTHER_PLUGIN_ORG_RE = /^(@(?!babel\/)[^/]+\/)(?![^/]*babel-plugin(?:-|\/|$)|[^/]+\/)/;
const OTHER_PRESET_ORG_RE = /^(@(?!babel\/)[^/]+\/)(?![^/]*babel-preset(?:-|\/|$)|[^/]+\/)/;
const OTHER_ORG_DEFAULT_RE = /^(@(?!babel$)[^/]+)$/;

function resolvePlugin(name, dirname) {
  return resolveStandardizedName("plugin", name, dirname);
}

function resolvePreset(name, dirname) {
  return resolveStandardizedName("preset", name, dirname);
}

function* loadPlugin(name, dirname) {
  const filepath = resolvePlugin(name, dirname);

  if (!filepath) {
    throw new Error(`Plugin ${name} not found relative to ${dirname}`);
  }

  const value = yield* requireModule("plugin", filepath);
  debug("Loaded plugin %o from %o.", name, dirname);
  return {
    filepath,
    value
  };
}

function* loadPreset(name, dirname) {
  const filepath = resolvePreset(name, dirname);

  if (!filepath) {
    throw new Error(`Preset ${name} not found relative to ${dirname}`);
  }

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

function resolveStandardizedName(type, name, dirname = process.cwd()) {
  const standardizedName = standardizeName(type, name);

  try {
    return (((v, w) => (v = v.split("."), w = w.split("."), +v[0] > +w[0] || v[0] == w[0] && +v[1] >= +w[1]))(process.versions.node, "8.9") ? require.resolve : (r, {
      paths: [b]
    }, M = require("module")) => {
      let f = M._findPath(r, M._nodeModulePaths(b).concat(b));

      if (f) return f;
      f = new Error(`Cannot resolve module '${r}'`);
      f.code = "MODULE_NOT_FOUND";
      throw f;
    })(standardizedName, {
      paths: [dirname]
    });
  } catch (e) {
    if (e.code !== "MODULE_NOT_FOUND") throw e;

    if (standardizedName !== name) {
      let resolvedOriginal = false;

      try {
        (((v, w) => (v = v.split("."), w = w.split("."), +v[0] > +w[0] || v[0] == w[0] && +v[1] >= +w[1]))(process.versions.node, "8.9") ? require.resolve : (r, {
          paths: [b]
        }, M = require("module")) => {
          let f = M._findPath(r, M._nodeModulePaths(b).concat(b));

          if (f) return f;
          f = new Error(`Cannot resolve module '${r}'`);
          f.code = "MODULE_NOT_FOUND";
          throw f;
        })(name, {
          paths: [dirname]
        });
        resolvedOriginal = true;
      } catch (_unused) {}

      if (resolvedOriginal) {
        e.message += `\n- If you want to resolve "${name}", use "module:${name}"`;
      }
    }

    let resolvedBabel = false;

    try {
      (((v, w) => (v = v.split("."), w = w.split("."), +v[0] > +w[0] || v[0] == w[0] && +v[1] >= +w[1]))(process.versions.node, "8.9") ? require.resolve : (r, {
        paths: [b]
      }, M = require("module")) => {
        let f = M._findPath(r, M._nodeModulePaths(b).concat(b));

        if (f) return f;
        f = new Error(`Cannot resolve module '${r}'`);
        f.code = "MODULE_NOT_FOUND";
        throw f;
      })(standardizeName(type, "@babel/" + name), {
        paths: [dirname]
      });
      resolvedBabel = true;
    } catch (_unused2) {}

    if (resolvedBabel) {
      e.message += `\n- Did you mean "@babel/${name}"?`;
    }

    let resolvedOppositeType = false;
    const oppositeType = type === "preset" ? "plugin" : "preset";

    try {
      (((v, w) => (v = v.split("."), w = w.split("."), +v[0] > +w[0] || v[0] == w[0] && +v[1] >= +w[1]))(process.versions.node, "8.9") ? require.resolve : (r, {
        paths: [b]
      }, M = require("module")) => {
        let f = M._findPath(r, M._nodeModulePaths(b).concat(b));

        if (f) return f;
        f = new Error(`Cannot resolve module '${r}'`);
        f.code = "MODULE_NOT_FOUND";
        throw f;
      })(standardizeName(oppositeType, name), {
        paths: [dirname]
      });
      resolvedOppositeType = true;
    } catch (_unused3) {}

    if (resolvedOppositeType) {
      e.message += `\n- Did you accidentally pass a ${oppositeType} as a ${type}?`;
    }

    throw e;
  }
}

const LOADING_MODULES = new Set();

function* requireModule(type, name) {
  if (LOADING_MODULES.has(name)) {
    throw new Error(`Reentrant ${type} detected trying to load "${name}". This module is not ignored ` + "and is trying to load itself while compiling itself, leading to a dependency cycle. " + 'We recommend adding it to your "ignore" list in your babelrc, or to a .babelignore.');
  }

  try {
    LOADING_MODULES.add(name);
    return yield* (0, _moduleTypes.default)(name, `You appear to be using a native ECMAScript module ${type}, ` + "which is only supported when running Babel asynchronously.", true);
  } catch (err) {
    err.message = `[BABEL]: ${err.message} (While processing: ${name})`;
    throw err;
  } finally {
    LOADING_MODULES.delete(name);
  }
}