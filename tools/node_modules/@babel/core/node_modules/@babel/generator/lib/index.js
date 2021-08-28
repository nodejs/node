"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = generate;
exports.CodeGenerator = void 0;

var _sourceMap = require("./source-map");

var _printer = require("./printer");

class Generator extends _printer.default {
  constructor(ast, opts = {}, code) {
    const format = normalizeOptions(code, opts);
    const map = opts.sourceMaps ? new _sourceMap.default(opts, code) : null;
    super(format, map);
    this.ast = void 0;
    this.ast = ast;
  }

  generate() {
    return super.generate(this.ast);
  }

}

function normalizeOptions(code, opts) {
  const format = {
    auxiliaryCommentBefore: opts.auxiliaryCommentBefore,
    auxiliaryCommentAfter: opts.auxiliaryCommentAfter,
    shouldPrintComment: opts.shouldPrintComment,
    retainLines: opts.retainLines,
    retainFunctionParens: opts.retainFunctionParens,
    comments: opts.comments == null || opts.comments,
    compact: opts.compact,
    minified: opts.minified,
    concise: opts.concise,
    indent: {
      adjustMultilineComment: true,
      style: "  ",
      base: 0
    },
    decoratorsBeforeExport: !!opts.decoratorsBeforeExport,
    jsescOption: Object.assign({
      quotes: "double",
      wrap: true,
      minimal: false
    }, opts.jsescOption),
    recordAndTupleSyntaxType: opts.recordAndTupleSyntaxType,
    topicToken: opts.topicToken
  };
  {
    format.jsonCompatibleStrings = opts.jsonCompatibleStrings;
  }

  if (format.minified) {
    format.compact = true;

    format.shouldPrintComment = format.shouldPrintComment || (() => format.comments);
  } else {
    format.shouldPrintComment = format.shouldPrintComment || (value => format.comments || value.indexOf("@license") >= 0 || value.indexOf("@preserve") >= 0);
  }

  if (format.compact === "auto") {
    format.compact = code.length > 500000;

    if (format.compact) {
      console.error("[BABEL] Note: The code generator has deoptimised the styling of " + `${opts.filename} as it exceeds the max of ${"500KB"}.`);
    }
  }

  if (format.compact) {
    format.indent.adjustMultilineComment = false;
  }

  return format;
}

class CodeGenerator {
  constructor(ast, opts, code) {
    this._generator = void 0;
    this._generator = new Generator(ast, opts, code);
  }

  generate() {
    return this._generator.generate();
  }

}

exports.CodeGenerator = CodeGenerator;

function generate(ast, opts, code) {
  const gen = new Generator(ast, opts, code);
  return gen.generate();
}