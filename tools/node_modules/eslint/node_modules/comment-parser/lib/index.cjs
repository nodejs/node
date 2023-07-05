"use strict";

var __createBinding = this && this.__createBinding || (Object.create ? function (o, m, k, k2) {
  if (k2 === undefined) k2 = k;
  Object.defineProperty(o, k2, {
    enumerable: true,
    get: function () {
      return m[k];
    }
  });
} : function (o, m, k, k2) {
  if (k2 === undefined) k2 = k;
  o[k2] = m[k];
});

var __exportStar = this && this.__exportStar || function (m, exports) {
  for (var p in m) if (p !== "default" && !Object.prototype.hasOwnProperty.call(exports, p)) __createBinding(exports, m, p);
};

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.util = exports.tokenizers = exports.transforms = exports.inspect = exports.stringify = exports.parse = void 0;

const index_1 = require("./parser/index.cjs");

const description_1 = require("./parser/tokenizers/description.cjs");

const name_1 = require("./parser/tokenizers/name.cjs");

const tag_1 = require("./parser/tokenizers/tag.cjs");

const type_1 = require("./parser/tokenizers/type.cjs");

const index_2 = require("./stringifier/index.cjs");

const align_1 = require("./transforms/align.cjs");

const indent_1 = require("./transforms/indent.cjs");

const crlf_1 = require("./transforms/crlf.cjs");

const index_3 = require("./transforms/index.cjs");

const util_1 = require("./util.cjs");

__exportStar(require("./primitives.cjs"), exports);

function parse(source, options = {}) {
  return index_1.default(options)(source);
}

exports.parse = parse;
exports.stringify = index_2.default();

var inspect_1 = require("./stringifier/inspect.cjs");

Object.defineProperty(exports, "inspect", {
  enumerable: true,
  get: function () {
    return inspect_1.default;
  }
});
exports.transforms = {
  flow: index_3.flow,
  align: align_1.default,
  indent: indent_1.default,
  crlf: crlf_1.default
};
exports.tokenizers = {
  tag: tag_1.default,
  type: type_1.default,
  name: name_1.default,
  description: description_1.default
};
exports.util = {
  rewireSpecs: util_1.rewireSpecs,
  rewireSource: util_1.rewireSource,
  seedBlock: util_1.seedBlock,
  seedTokens: util_1.seedTokens
};
//# sourceMappingURL=index.cjs.map
