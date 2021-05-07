"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.findConfigUpwards = findConfigUpwards;
exports.findRelativeConfig = findRelativeConfig;
exports.findRootConfig = findRootConfig;
exports.loadConfig = loadConfig;
exports.resolveShowConfigPath = resolveShowConfigPath;
exports.ROOT_CONFIG_FILENAMES = void 0;

function _debug() {
  const data = _interopRequireDefault(require("debug"));

  _debug = function () {
    return data;
  };

  return data;
}

function _path() {
  const data = _interopRequireDefault(require("path"));

  _path = function () {
    return data;
  };

  return data;
}

function _json() {
  const data = _interopRequireDefault(require("json5"));

  _json = function () {
    return data;
  };

  return data;
}

function _gensync() {
  const data = _interopRequireDefault(require("gensync"));

  _gensync = function () {
    return data;
  };

  return data;
}

var _caching = require("../caching");

var _configApi = _interopRequireDefault(require("../helpers/config-api"));

var _utils = require("./utils");

var _moduleTypes = _interopRequireDefault(require("./module-types"));

var _patternToRegex = _interopRequireDefault(require("../pattern-to-regex"));

var fs = _interopRequireWildcard(require("../../gensync-utils/fs"));

function _getRequireWildcardCache() { if (typeof WeakMap !== "function") return null; var cache = new WeakMap(); _getRequireWildcardCache = function () { return cache; }; return cache; }

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } if (obj === null || typeof obj !== "object" && typeof obj !== "function") { return { default: obj }; } var cache = _getRequireWildcardCache(); if (cache && cache.has(obj)) { return cache.get(obj); } var newObj = {}; var hasPropertyDescriptor = Object.defineProperty && Object.getOwnPropertyDescriptor; for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = hasPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : null; if (desc && (desc.get || desc.set)) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } newObj.default = obj; if (cache) { cache.set(obj, newObj); } return newObj; }

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

const debug = (0, _debug().default)("babel:config:loading:files:configuration");
const ROOT_CONFIG_FILENAMES = ["babel.config.js", "babel.config.cjs", "babel.config.mjs", "babel.config.json"];
exports.ROOT_CONFIG_FILENAMES = ROOT_CONFIG_FILENAMES;
const RELATIVE_CONFIG_FILENAMES = [".babelrc", ".babelrc.js", ".babelrc.cjs", ".babelrc.mjs", ".babelrc.json"];
const BABELIGNORE_FILENAME = ".babelignore";

function* findConfigUpwards(rootDir) {
  let dirname = rootDir;

  while (true) {
    for (const filename of ROOT_CONFIG_FILENAMES) {
      if (yield* fs.exists(_path().default.join(dirname, filename))) {
        return dirname;
      }
    }

    const nextDir = _path().default.dirname(dirname);

    if (dirname === nextDir) break;
    dirname = nextDir;
  }

  return null;
}

function* findRelativeConfig(packageData, envName, caller) {
  let config = null;
  let ignore = null;

  const dirname = _path().default.dirname(packageData.filepath);

  for (const loc of packageData.directories) {
    if (!config) {
      var _packageData$pkg;

      config = yield* loadOneConfig(RELATIVE_CONFIG_FILENAMES, loc, envName, caller, ((_packageData$pkg = packageData.pkg) == null ? void 0 : _packageData$pkg.dirname) === loc ? packageToBabelConfig(packageData.pkg) : null);
    }

    if (!ignore) {
      const ignoreLoc = _path().default.join(loc, BABELIGNORE_FILENAME);

      ignore = yield* readIgnoreConfig(ignoreLoc);

      if (ignore) {
        debug("Found ignore %o from %o.", ignore.filepath, dirname);
      }
    }
  }

  return {
    config,
    ignore
  };
}

function findRootConfig(dirname, envName, caller) {
  return loadOneConfig(ROOT_CONFIG_FILENAMES, dirname, envName, caller);
}

function* loadOneConfig(names, dirname, envName, caller, previousConfig = null) {
  const configs = yield* _gensync().default.all(names.map(filename => readConfig(_path().default.join(dirname, filename), envName, caller)));
  const config = configs.reduce((previousConfig, config) => {
    if (config && previousConfig) {
      throw new Error(`Multiple configuration files found. Please remove one:\n` + ` - ${_path().default.basename(previousConfig.filepath)}\n` + ` - ${config.filepath}\n` + `from ${dirname}`);
    }

    return config || previousConfig;
  }, previousConfig);

  if (config) {
    debug("Found configuration %o from %o.", config.filepath, dirname);
  }

  return config;
}

function* loadConfig(name, dirname, envName, caller) {
  const filepath = (parseFloat(process.versions.node) >= 8.9 ? require.resolve : (r, {
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
  const conf = yield* readConfig(filepath, envName, caller);

  if (!conf) {
    throw new Error(`Config file ${filepath} contains no configuration data`);
  }

  debug("Loaded config %o from %o.", name, dirname);
  return conf;
}

function readConfig(filepath, envName, caller) {
  const ext = _path().default.extname(filepath);

  return ext === ".js" || ext === ".cjs" || ext === ".mjs" ? readConfigJS(filepath, {
    envName,
    caller
  }) : readConfigJSON5(filepath);
}

const LOADING_CONFIGS = new Set();
const readConfigJS = (0, _caching.makeStrongCache)(function* readConfigJS(filepath, cache) {
  if (!fs.exists.sync(filepath)) {
    cache.forever();
    return null;
  }

  if (LOADING_CONFIGS.has(filepath)) {
    cache.never();
    debug("Auto-ignoring usage of config %o.", filepath);
    return {
      filepath,
      dirname: _path().default.dirname(filepath),
      options: {}
    };
  }

  let options;

  try {
    LOADING_CONFIGS.add(filepath);
    options = yield* (0, _moduleTypes.default)(filepath, "You appear to be using a native ECMAScript module configuration " + "file, which is only supported when running Babel asynchronously.");
  } catch (err) {
    err.message = `${filepath}: Error while loading config - ${err.message}`;
    throw err;
  } finally {
    LOADING_CONFIGS.delete(filepath);
  }

  let assertCache = false;

  if (typeof options === "function") {
    yield* [];
    options = options((0, _configApi.default)(cache));
    assertCache = true;
  }

  if (!options || typeof options !== "object" || Array.isArray(options)) {
    throw new Error(`${filepath}: Configuration should be an exported JavaScript object.`);
  }

  if (typeof options.then === "function") {
    throw new Error(`You appear to be using an async configuration, ` + `which your current version of Babel does not support. ` + `We may add support for this in the future, ` + `but if you're on the most recent version of @babel/core and still ` + `seeing this error, then you'll need to synchronously return your config.`);
  }

  if (assertCache && !cache.configured()) throwConfigError();
  return {
    filepath,
    dirname: _path().default.dirname(filepath),
    options
  };
});
const packageToBabelConfig = (0, _caching.makeWeakCacheSync)(file => {
  const babel = file.options["babel"];
  if (typeof babel === "undefined") return null;

  if (typeof babel !== "object" || Array.isArray(babel) || babel === null) {
    throw new Error(`${file.filepath}: .babel property must be an object`);
  }

  return {
    filepath: file.filepath,
    dirname: file.dirname,
    options: babel
  };
});
const readConfigJSON5 = (0, _utils.makeStaticFileCache)((filepath, content) => {
  let options;

  try {
    options = _json().default.parse(content);
  } catch (err) {
    err.message = `${filepath}: Error while parsing config - ${err.message}`;
    throw err;
  }

  if (!options) throw new Error(`${filepath}: No config detected`);

  if (typeof options !== "object") {
    throw new Error(`${filepath}: Config returned typeof ${typeof options}`);
  }

  if (Array.isArray(options)) {
    throw new Error(`${filepath}: Expected config object but found array`);
  }

  return {
    filepath,
    dirname: _path().default.dirname(filepath),
    options
  };
});
const readIgnoreConfig = (0, _utils.makeStaticFileCache)((filepath, content) => {
  const ignoreDir = _path().default.dirname(filepath);

  const ignorePatterns = content.split("\n").map(line => line.replace(/#(.*?)$/, "").trim()).filter(line => !!line);

  for (const pattern of ignorePatterns) {
    if (pattern[0] === "!") {
      throw new Error(`Negation of file paths is not supported.`);
    }
  }

  return {
    filepath,
    dirname: _path().default.dirname(filepath),
    ignore: ignorePatterns.map(pattern => (0, _patternToRegex.default)(pattern, ignoreDir))
  };
});

function* resolveShowConfigPath(dirname) {
  const targetPath = process.env.BABEL_SHOW_CONFIG_FOR;

  if (targetPath != null) {
    const absolutePath = _path().default.resolve(dirname, targetPath);

    const stats = yield* fs.stat(absolutePath);

    if (!stats.isFile()) {
      throw new Error(`${absolutePath}: BABEL_SHOW_CONFIG_FOR must refer to a regular file, directories are not supported.`);
    }

    return absolutePath;
  }

  return null;
}

function throwConfigError() {
  throw new Error(`\
Caching was left unconfigured. Babel's plugins, presets, and .babelrc.js files can be configured
for various types of caching, using the first param of their handler functions:

module.exports = function(api) {
  // The API exposes the following:

  // Cache the returned value forever and don't call this function again.
  api.cache(true);

  // Don't cache at all. Not recommended because it will be very slow.
  api.cache(false);

  // Cached based on the value of some function. If this function returns a value different from
  // a previously-encountered value, the plugins will re-evaluate.
  var env = api.cache(() => process.env.NODE_ENV);

  // If testing for a specific env, we recommend specifics to avoid instantiating a plugin for
  // any possible NODE_ENV value that might come up during plugin execution.
  var isProd = api.cache(() => process.env.NODE_ENV === "production");

  // .cache(fn) will perform a linear search though instances to find the matching plugin based
  // based on previous instantiated plugins. If you want to recreate the plugin and discard the
  // previous instance whenever something changes, you may use:
  var isProd = api.cache.invalidate(() => process.env.NODE_ENV === "production");

  // Note, we also expose the following more-verbose versions of the above examples:
  api.cache.forever(); // api.cache(true)
  api.cache.never();   // api.cache(false)
  api.cache.using(fn); // api.cache(fn)

  // Return the value that will be cached.
  return { };
};`);
}