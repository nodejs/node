"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.parseSync = exports.parseAsync = exports.parse = void 0;

function _gensync() {
  const data = require("gensync");

  _gensync = function () {
    return data;
  };

  return data;
}

var _config = require("./config");

var _parser = require("./parser");

var _normalizeOpts = require("./transformation/normalize-opts");

const parseRunner = _gensync()(function* parse(code, opts) {
  const config = yield* (0, _config.default)(opts);

  if (config === null) {
    return null;
  }

  return yield* (0, _parser.default)(config.passes, (0, _normalizeOpts.default)(config), code);
});

const parse = function parse(code, opts, callback) {
  if (typeof opts === "function") {
    callback = opts;
    opts = undefined;
  }

  if (callback === undefined) {
    {
      return parseRunner.sync(code, opts);
    }
  }

  parseRunner.errback(code, opts, callback);
};

exports.parse = parse;
const parseSync = parseRunner.sync;
exports.parseSync = parseSync;
const parseAsync = parseRunner.async;
exports.parseAsync = parseAsync;
0 && 0;