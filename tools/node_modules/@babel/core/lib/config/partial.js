"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = loadPrivatePartialConfig;
exports.loadPartialConfig = void 0;

function _path() {
  const data = _interopRequireDefault(require("path"));

  _path = function () {
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

var _plugin = _interopRequireDefault(require("./plugin"));

var _util = require("./util");

var _item = require("./item");

var _configChain = require("./config-chain");

var _environment = require("./helpers/environment");

var _options = require("./validation/options");

var _files = require("./files");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _objectWithoutPropertiesLoose(source, excluded) { if (source == null) return {}; var target = {}; var sourceKeys = Object.keys(source); var key, i; for (i = 0; i < sourceKeys.length; i++) { key = sourceKeys[i]; if (excluded.indexOf(key) >= 0) continue; target[key] = source[key]; } return target; }

function* resolveRootMode(rootDir, rootMode) {
  switch (rootMode) {
    case "root":
      return rootDir;

    case "upward-optional":
      {
        const upwardRootDir = yield* (0, _files.findConfigUpwards)(rootDir);
        return upwardRootDir === null ? rootDir : upwardRootDir;
      }

    case "upward":
      {
        const upwardRootDir = yield* (0, _files.findConfigUpwards)(rootDir);
        if (upwardRootDir !== null) return upwardRootDir;
        throw Object.assign(new Error(`Babel was run with rootMode:"upward" but a root could not ` + `be found when searching upward from "${rootDir}".\n` + `One of the following config files must be in the directory tree: ` + `"${_files.ROOT_CONFIG_FILENAMES.join(", ")}".`), {
          code: "BABEL_ROOT_NOT_FOUND",
          dirname: rootDir
        });
      }

    default:
      throw new Error(`Assertion failure - unknown rootMode value.`);
  }
}

function* loadPrivatePartialConfig(inputOpts) {
  if (inputOpts != null && (typeof inputOpts !== "object" || Array.isArray(inputOpts))) {
    throw new Error("Babel options must be an object, null, or undefined");
  }

  const args = inputOpts ? (0, _options.validate)("arguments", inputOpts) : {};
  const {
    envName = (0, _environment.getEnv)(),
    cwd = ".",
    root: rootDir = ".",
    rootMode = "root",
    caller,
    cloneInputAst = true
  } = args;

  const absoluteCwd = _path().default.resolve(cwd);

  const absoluteRootDir = yield* resolveRootMode(_path().default.resolve(absoluteCwd, rootDir), rootMode);
  const filename = typeof args.filename === "string" ? _path().default.resolve(cwd, args.filename) : undefined;
  const showConfigPath = yield* (0, _files.resolveShowConfigPath)(absoluteCwd);
  const context = {
    filename,
    cwd: absoluteCwd,
    root: absoluteRootDir,
    envName,
    caller,
    showConfig: showConfigPath === filename
  };
  const configChain = yield* (0, _configChain.buildRootChain)(args, context);
  if (!configChain) return null;
  const options = {};
  configChain.options.forEach(opts => {
    (0, _util.mergeOptions)(options, opts);
  });
  options.cloneInputAst = cloneInputAst;
  options.babelrc = false;
  options.configFile = false;
  options.passPerPreset = false;
  options.envName = context.envName;
  options.cwd = context.cwd;
  options.root = context.root;
  options.filename = typeof context.filename === "string" ? context.filename : undefined;
  options.plugins = configChain.plugins.map(descriptor => (0, _item.createItemFromDescriptor)(descriptor));
  options.presets = configChain.presets.map(descriptor => (0, _item.createItemFromDescriptor)(descriptor));
  return {
    options,
    context,
    fileHandling: configChain.fileHandling,
    ignore: configChain.ignore,
    babelrc: configChain.babelrc,
    config: configChain.config,
    files: configChain.files
  };
}

const loadPartialConfig = (0, _gensync().default)(function* (opts) {
  let showIgnoredFiles = false;

  if (typeof opts === "object" && opts !== null && !Array.isArray(opts)) {
    var _opts = opts;
    ({
      showIgnoredFiles
    } = _opts);
    opts = _objectWithoutPropertiesLoose(_opts, ["showIgnoredFiles"]);
    _opts;
  }

  const result = yield* loadPrivatePartialConfig(opts);
  if (!result) return null;
  const {
    options,
    babelrc,
    ignore,
    config,
    fileHandling,
    files
  } = result;

  if (fileHandling === "ignored" && !showIgnoredFiles) {
    return null;
  }

  (options.plugins || []).forEach(item => {
    if (item.value instanceof _plugin.default) {
      throw new Error("Passing cached plugin instances is not supported in " + "babel.loadPartialConfig()");
    }
  });
  return new PartialConfig(options, babelrc ? babelrc.filepath : undefined, ignore ? ignore.filepath : undefined, config ? config.filepath : undefined, fileHandling, files);
});
exports.loadPartialConfig = loadPartialConfig;

class PartialConfig {
  constructor(options, babelrc, ignore, config, fileHandling, files) {
    this.options = void 0;
    this.babelrc = void 0;
    this.babelignore = void 0;
    this.config = void 0;
    this.fileHandling = void 0;
    this.files = void 0;
    this.options = options;
    this.babelignore = ignore;
    this.babelrc = babelrc;
    this.config = config;
    this.fileHandling = fileHandling;
    this.files = files;
    Object.freeze(this);
  }

  hasFilesystemConfig() {
    return this.babelrc !== undefined || this.config !== undefined;
  }

}

Object.freeze(PartialConfig.prototype);