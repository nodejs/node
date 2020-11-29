"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
Object.defineProperty(exports, "default", {
  enumerable: true,
  get: function () {
    return _full.default;
  }
});
exports.loadOptionsAsync = exports.loadOptionsSync = exports.loadOptions = exports.loadPartialConfigAsync = exports.loadPartialConfigSync = exports.loadPartialConfig = void 0;

function _gensync() {
  const data = _interopRequireDefault(require("gensync"));

  _gensync = function () {
    return data;
  };

  return data;
}

var _full = _interopRequireDefault(require("./full"));

var _partial = require("./partial");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

const loadOptionsRunner = (0, _gensync().default)(function* (opts) {
  var _config$options;

  const config = yield* (0, _full.default)(opts);
  return (_config$options = config == null ? void 0 : config.options) != null ? _config$options : null;
});

const maybeErrback = runner => (opts, callback) => {
  if (callback === undefined && typeof opts === "function") {
    callback = opts;
    opts = undefined;
  }

  return callback ? runner.errback(opts, callback) : runner.sync(opts);
};

const loadPartialConfig = maybeErrback(_partial.loadPartialConfig);
exports.loadPartialConfig = loadPartialConfig;
const loadPartialConfigSync = _partial.loadPartialConfig.sync;
exports.loadPartialConfigSync = loadPartialConfigSync;
const loadPartialConfigAsync = _partial.loadPartialConfig.async;
exports.loadPartialConfigAsync = loadPartialConfigAsync;
const loadOptions = maybeErrback(loadOptionsRunner);
exports.loadOptions = loadOptions;
const loadOptionsSync = loadOptionsRunner.sync;
exports.loadOptionsSync = loadOptionsSync;
const loadOptionsAsync = loadOptionsRunner.async;
exports.loadOptionsAsync = loadOptionsAsync;