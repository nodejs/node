"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.DEFAULT_EXTENSIONS = void 0;
Object.defineProperty(exports, "File", {
  enumerable: true,
  get: function () {
    return _file.default;
  }
});
exports.OptionManager = void 0;
exports.Plugin = Plugin;
Object.defineProperty(exports, "buildExternalHelpers", {
  enumerable: true,
  get: function () {
    return _buildExternalHelpers.default;
  }
});
Object.defineProperty(exports, "createConfigItem", {
  enumerable: true,
  get: function () {
    return _config.createConfigItem;
  }
});
Object.defineProperty(exports, "createConfigItemAsync", {
  enumerable: true,
  get: function () {
    return _config.createConfigItemAsync;
  }
});
Object.defineProperty(exports, "createConfigItemSync", {
  enumerable: true,
  get: function () {
    return _config.createConfigItemSync;
  }
});
Object.defineProperty(exports, "getEnv", {
  enumerable: true,
  get: function () {
    return _environment.getEnv;
  }
});
Object.defineProperty(exports, "loadOptions", {
  enumerable: true,
  get: function () {
    return _config.loadOptions;
  }
});
Object.defineProperty(exports, "loadOptionsAsync", {
  enumerable: true,
  get: function () {
    return _config.loadOptionsAsync;
  }
});
Object.defineProperty(exports, "loadOptionsSync", {
  enumerable: true,
  get: function () {
    return _config.loadOptionsSync;
  }
});
Object.defineProperty(exports, "loadPartialConfig", {
  enumerable: true,
  get: function () {
    return _config.loadPartialConfig;
  }
});
Object.defineProperty(exports, "loadPartialConfigAsync", {
  enumerable: true,
  get: function () {
    return _config.loadPartialConfigAsync;
  }
});
Object.defineProperty(exports, "loadPartialConfigSync", {
  enumerable: true,
  get: function () {
    return _config.loadPartialConfigSync;
  }
});
Object.defineProperty(exports, "parse", {
  enumerable: true,
  get: function () {
    return _parse.parse;
  }
});
Object.defineProperty(exports, "parseAsync", {
  enumerable: true,
  get: function () {
    return _parse.parseAsync;
  }
});
Object.defineProperty(exports, "parseSync", {
  enumerable: true,
  get: function () {
    return _parse.parseSync;
  }
});
Object.defineProperty(exports, "resolvePlugin", {
  enumerable: true,
  get: function () {
    return _files.resolvePlugin;
  }
});
Object.defineProperty(exports, "resolvePreset", {
  enumerable: true,
  get: function () {
    return _files.resolvePreset;
  }
});
Object.defineProperty(exports, "template", {
  enumerable: true,
  get: function () {
    return _template().default;
  }
});
Object.defineProperty(exports, "tokTypes", {
  enumerable: true,
  get: function () {
    return _parser().tokTypes;
  }
});
Object.defineProperty(exports, "transform", {
  enumerable: true,
  get: function () {
    return _transform.transform;
  }
});
Object.defineProperty(exports, "transformAsync", {
  enumerable: true,
  get: function () {
    return _transform.transformAsync;
  }
});
Object.defineProperty(exports, "transformFile", {
  enumerable: true,
  get: function () {
    return _transformFile.transformFile;
  }
});
Object.defineProperty(exports, "transformFileAsync", {
  enumerable: true,
  get: function () {
    return _transformFile.transformFileAsync;
  }
});
Object.defineProperty(exports, "transformFileSync", {
  enumerable: true,
  get: function () {
    return _transformFile.transformFileSync;
  }
});
Object.defineProperty(exports, "transformFromAst", {
  enumerable: true,
  get: function () {
    return _transformAst.transformFromAst;
  }
});
Object.defineProperty(exports, "transformFromAstAsync", {
  enumerable: true,
  get: function () {
    return _transformAst.transformFromAstAsync;
  }
});
Object.defineProperty(exports, "transformFromAstSync", {
  enumerable: true,
  get: function () {
    return _transformAst.transformFromAstSync;
  }
});
Object.defineProperty(exports, "transformSync", {
  enumerable: true,
  get: function () {
    return _transform.transformSync;
  }
});
Object.defineProperty(exports, "traverse", {
  enumerable: true,
  get: function () {
    return _traverse().default;
  }
});
exports.version = exports.types = void 0;

var _file = require("./transformation/file/file");

var _buildExternalHelpers = require("./tools/build-external-helpers");

var _files = require("./config/files");

var _environment = require("./config/helpers/environment");

function _types() {
  const data = require("@babel/types");

  _types = function () {
    return data;
  };

  return data;
}

Object.defineProperty(exports, "types", {
  enumerable: true,
  get: function () {
    return _types();
  }
});

function _parser() {
  const data = require("@babel/parser");

  _parser = function () {
    return data;
  };

  return data;
}

function _traverse() {
  const data = require("@babel/traverse");

  _traverse = function () {
    return data;
  };

  return data;
}

function _template() {
  const data = require("@babel/template");

  _template = function () {
    return data;
  };

  return data;
}

var _config = require("./config");

var _transform = require("./transform");

var _transformFile = require("./transform-file");

var _transformAst = require("./transform-ast");

var _parse = require("./parse");

const version = "7.17.5";
exports.version = version;
const DEFAULT_EXTENSIONS = Object.freeze([".js", ".jsx", ".es6", ".es", ".mjs", ".cjs"]);
exports.DEFAULT_EXTENSIONS = DEFAULT_EXTENSIONS;

class OptionManager {
  init(opts) {
    return (0, _config.loadOptions)(opts);
  }

}

exports.OptionManager = OptionManager;

function Plugin(alias) {
  throw new Error(`The (${alias}) Babel 5 plugin is being run with an unsupported Babel version.`);
}