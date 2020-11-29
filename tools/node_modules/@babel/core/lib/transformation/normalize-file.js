"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = normalizeFile;

function _fs() {
  const data = _interopRequireDefault(require("fs"));

  _fs = function () {
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

function _debug() {
  const data = _interopRequireDefault(require("debug"));

  _debug = function () {
    return data;
  };

  return data;
}

function _cloneDeep() {
  const data = _interopRequireDefault(require("lodash/cloneDeep"));

  _cloneDeep = function () {
    return data;
  };

  return data;
}

function t() {
  const data = _interopRequireWildcard(require("@babel/types"));

  t = function () {
    return data;
  };

  return data;
}

function _convertSourceMap() {
  const data = _interopRequireDefault(require("convert-source-map"));

  _convertSourceMap = function () {
    return data;
  };

  return data;
}

var _file = _interopRequireDefault(require("./file/file"));

var _parser = _interopRequireDefault(require("../parser"));

function _getRequireWildcardCache() { if (typeof WeakMap !== "function") return null; var cache = new WeakMap(); _getRequireWildcardCache = function () { return cache; }; return cache; }

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } if (obj === null || typeof obj !== "object" && typeof obj !== "function") { return { default: obj }; } var cache = _getRequireWildcardCache(); if (cache && cache.has(obj)) { return cache.get(obj); } var newObj = {}; var hasPropertyDescriptor = Object.defineProperty && Object.getOwnPropertyDescriptor; for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = hasPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : null; if (desc && (desc.get || desc.set)) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } newObj.default = obj; if (cache) { cache.set(obj, newObj); } return newObj; }

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

const debug = (0, _debug().default)("babel:transform:file");
const LARGE_INPUT_SOURCEMAP_THRESHOLD = 1000000;

function* normalizeFile(pluginPasses, options, code, ast) {
  code = `${code || ""}`;

  if (ast) {
    if (ast.type === "Program") {
      ast = t().file(ast, [], []);
    } else if (ast.type !== "File") {
      throw new Error("AST root must be a Program or File node");
    }

    const {
      cloneInputAst
    } = options;

    if (cloneInputAst) {
      ast = (0, _cloneDeep().default)(ast);
    }
  } else {
    ast = yield* (0, _parser.default)(pluginPasses, options, code);
  }

  let inputMap = null;

  if (options.inputSourceMap !== false) {
    if (typeof options.inputSourceMap === "object") {
      inputMap = _convertSourceMap().default.fromObject(options.inputSourceMap);
    }

    if (!inputMap) {
      const lastComment = extractComments(INLINE_SOURCEMAP_REGEX, ast);

      if (lastComment) {
        try {
          inputMap = _convertSourceMap().default.fromComment(lastComment);
        } catch (err) {
          debug("discarding unknown inline input sourcemap", err);
        }
      }
    }

    if (!inputMap) {
      const lastComment = extractComments(EXTERNAL_SOURCEMAP_REGEX, ast);

      if (typeof options.filename === "string" && lastComment) {
        try {
          const match = EXTERNAL_SOURCEMAP_REGEX.exec(lastComment);

          const inputMapContent = _fs().default.readFileSync(_path().default.resolve(_path().default.dirname(options.filename), match[1]));

          if (inputMapContent.length > LARGE_INPUT_SOURCEMAP_THRESHOLD) {
            debug("skip merging input map > 1 MB");
          } else {
            inputMap = _convertSourceMap().default.fromJSON(inputMapContent);
          }
        } catch (err) {
          debug("discarding unknown file input sourcemap", err);
        }
      } else if (lastComment) {
        debug("discarding un-loadable file input sourcemap");
      }
    }
  }

  return new _file.default(options, {
    code,
    ast,
    inputMap
  });
}

const INLINE_SOURCEMAP_REGEX = /^[@#]\s+sourceMappingURL=data:(?:application|text)\/json;(?:charset[:=]\S+?;)?base64,(?:.*)$/;
const EXTERNAL_SOURCEMAP_REGEX = /^[@#][ \t]+sourceMappingURL=([^\s'"`]+)[ \t]*$/;

function extractCommentsFromList(regex, comments, lastComment) {
  if (comments) {
    comments = comments.filter(({
      value
    }) => {
      if (regex.test(value)) {
        lastComment = value;
        return false;
      }

      return true;
    });
  }

  return [comments, lastComment];
}

function extractComments(regex, ast) {
  let lastComment = null;
  t().traverseFast(ast, node => {
    [node.leadingComments, lastComment] = extractCommentsFromList(regex, node.leadingComments, lastComment);
    [node.innerComments, lastComment] = extractCommentsFromList(regex, node.innerComments, lastComment);
    [node.trailingComments, lastComment] = extractCommentsFromList(regex, node.trailingComments, lastComment);
  });
  return lastComment;
}