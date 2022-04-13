"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

var _buffer = require("./buffer");

var n = require("./node");

var _t = require("@babel/types");

var generatorFunctions = require("./generators");

const {
  isProgram,
  isFile,
  isEmptyStatement
} = _t;
const SCIENTIFIC_NOTATION = /e/i;
const ZERO_DECIMAL_INTEGER = /\.0+$/;
const NON_DECIMAL_LITERAL = /^0[box]/;
const PURE_ANNOTATION_RE = /^\s*[@#]__PURE__\s*$/;
const {
  needsParens,
  needsWhitespaceAfter,
  needsWhitespaceBefore
} = n;

class Printer {
  constructor(format, map) {
    this.inForStatementInitCounter = 0;
    this._printStack = [];
    this._indent = 0;
    this._insideAux = false;
    this._parenPushNewlineState = null;
    this._noLineTerminator = false;
    this._printAuxAfterOnNextUserNode = false;
    this._printedComments = new WeakSet();
    this._endsWithInteger = false;
    this._endsWithWord = false;
    this.format = format;
    this._buf = new _buffer.default(map);
  }

  generate(ast) {
    this.print(ast);

    this._maybeAddAuxComment();

    return this._buf.get();
  }

  indent() {
    if (this.format.compact || this.format.concise) return;
    this._indent++;
  }

  dedent() {
    if (this.format.compact || this.format.concise) return;
    this._indent--;
  }

  semicolon(force = false) {
    this._maybeAddAuxComment();

    this._append(";", !force);
  }

  rightBrace() {
    if (this.format.minified) {
      this._buf.removeLastSemicolon();
    }

    this.token("}");
  }

  space(force = false) {
    if (this.format.compact) return;

    if (force) {
      this._space();
    } else if (this._buf.hasContent()) {
      const lastCp = this.getLastChar();

      if (lastCp !== 32 && lastCp !== 10) {
        this._space();
      }
    }
  }

  word(str) {
    if (this._endsWithWord || this.endsWith(47) && str.charCodeAt(0) === 47) {
      this._space();
    }

    this._maybeAddAuxComment();

    this._append(str);

    this._endsWithWord = true;
  }

  number(str) {
    this.word(str);
    this._endsWithInteger = Number.isInteger(+str) && !NON_DECIMAL_LITERAL.test(str) && !SCIENTIFIC_NOTATION.test(str) && !ZERO_DECIMAL_INTEGER.test(str) && str.charCodeAt(str.length - 1) !== 46;
  }

  token(str) {
    const lastChar = this.getLastChar();
    const strFirst = str.charCodeAt(0);

    if (str === "--" && lastChar === 33 || strFirst === 43 && lastChar === 43 || strFirst === 45 && lastChar === 45 || strFirst === 46 && this._endsWithInteger) {
      this._space();
    }

    this._maybeAddAuxComment();

    this._append(str);
  }

  newline(i = 1) {
    if (this.format.retainLines || this.format.compact) return;

    if (this.format.concise) {
      this.space();
      return;
    }

    const charBeforeNewline = this.endsWithCharAndNewline();
    if (charBeforeNewline === 10) return;

    if (charBeforeNewline === 123 || charBeforeNewline === 58) {
      i--;
    }

    if (i <= 0) return;

    for (let j = 0; j < i; j++) {
      this._newline();
    }
  }

  endsWith(char) {
    return this.getLastChar() === char;
  }

  getLastChar() {
    return this._buf.getLastChar();
  }

  endsWithCharAndNewline() {
    return this._buf.endsWithCharAndNewline();
  }

  removeTrailingNewline() {
    this._buf.removeTrailingNewline();
  }

  exactSource(loc, cb) {
    this._catchUp("start", loc);

    this._buf.exactSource(loc, cb);
  }

  source(prop, loc) {
    this._catchUp(prop, loc);

    this._buf.source(prop, loc);
  }

  withSource(prop, loc, cb) {
    this._catchUp(prop, loc);

    this._buf.withSource(prop, loc, cb);
  }

  _space() {
    this._append(" ", true);
  }

  _newline() {
    this._append("\n", true);
  }

  _append(str, queue = false) {
    this._maybeAddParen(str);

    this._maybeIndent(str);

    if (queue) this._buf.queue(str);else this._buf.append(str);
    this._endsWithWord = false;
    this._endsWithInteger = false;
  }

  _maybeIndent(str) {
    if (this._indent && this.endsWith(10) && str.charCodeAt(0) !== 10) {
      this._buf.queue(this._getIndent());
    }
  }

  _maybeAddParen(str) {
    const parenPushNewlineState = this._parenPushNewlineState;
    if (!parenPushNewlineState) return;
    let i;

    for (i = 0; i < str.length && str[i] === " "; i++) continue;

    if (i === str.length) {
      return;
    }

    const cha = str[i];

    if (cha !== "\n") {
      if (cha !== "/" || i + 1 === str.length) {
        this._parenPushNewlineState = null;
        return;
      }

      const chaPost = str[i + 1];

      if (chaPost === "*") {
        if (PURE_ANNOTATION_RE.test(str.slice(i + 2, str.length - 2))) {
          return;
        }
      } else if (chaPost !== "/") {
        this._parenPushNewlineState = null;
        return;
      }
    }

    this.token("(");
    this.indent();
    parenPushNewlineState.printed = true;
  }

  _catchUp(prop, loc) {
    if (!this.format.retainLines) return;
    const pos = loc ? loc[prop] : null;

    if ((pos == null ? void 0 : pos.line) != null) {
      const count = pos.line - this._buf.getCurrentLine();

      for (let i = 0; i < count; i++) {
        this._newline();
      }
    }
  }

  _getIndent() {
    return this.format.indent.style.repeat(this._indent);
  }

  startTerminatorless(isLabel = false) {
    if (isLabel) {
      this._noLineTerminator = true;
      return null;
    } else {
      return this._parenPushNewlineState = {
        printed: false
      };
    }
  }

  endTerminatorless(state) {
    this._noLineTerminator = false;

    if (state != null && state.printed) {
      this.dedent();
      this.newline();
      this.token(")");
    }
  }

  print(node, parent) {
    if (!node) return;
    const oldConcise = this.format.concise;

    if (node._compact) {
      this.format.concise = true;
    }

    const printMethod = this[node.type];

    if (!printMethod) {
      throw new ReferenceError(`unknown node of type ${JSON.stringify(node.type)} with constructor ${JSON.stringify(node == null ? void 0 : node.constructor.name)}`);
    }

    this._printStack.push(node);

    const oldInAux = this._insideAux;
    this._insideAux = !node.loc;

    this._maybeAddAuxComment(this._insideAux && !oldInAux);

    let shouldPrintParens = needsParens(node, parent, this._printStack);

    if (this.format.retainFunctionParens && node.type === "FunctionExpression" && node.extra && node.extra.parenthesized) {
      shouldPrintParens = true;
    }

    if (shouldPrintParens) this.token("(");

    this._printLeadingComments(node);

    const loc = isProgram(node) || isFile(node) ? null : node.loc;
    this.withSource("start", loc, () => {
      printMethod.call(this, node, parent);
    });

    this._printTrailingComments(node);

    if (shouldPrintParens) this.token(")");

    this._printStack.pop();

    this.format.concise = oldConcise;
    this._insideAux = oldInAux;
  }

  _maybeAddAuxComment(enteredPositionlessNode) {
    if (enteredPositionlessNode) this._printAuxBeforeComment();
    if (!this._insideAux) this._printAuxAfterComment();
  }

  _printAuxBeforeComment() {
    if (this._printAuxAfterOnNextUserNode) return;
    this._printAuxAfterOnNextUserNode = true;
    const comment = this.format.auxiliaryCommentBefore;

    if (comment) {
      this._printComment({
        type: "CommentBlock",
        value: comment
      });
    }
  }

  _printAuxAfterComment() {
    if (!this._printAuxAfterOnNextUserNode) return;
    this._printAuxAfterOnNextUserNode = false;
    const comment = this.format.auxiliaryCommentAfter;

    if (comment) {
      this._printComment({
        type: "CommentBlock",
        value: comment
      });
    }
  }

  getPossibleRaw(node) {
    const extra = node.extra;

    if (extra && extra.raw != null && extra.rawValue != null && node.value === extra.rawValue) {
      return extra.raw;
    }
  }

  printJoin(nodes, parent, opts = {}) {
    if (!(nodes != null && nodes.length)) return;
    if (opts.indent) this.indent();
    const newlineOpts = {
      addNewlines: opts.addNewlines
    };

    for (let i = 0; i < nodes.length; i++) {
      const node = nodes[i];
      if (!node) continue;
      if (opts.statement) this._printNewline(true, node, parent, newlineOpts);
      this.print(node, parent);

      if (opts.iterator) {
        opts.iterator(node, i);
      }

      if (opts.separator && i < nodes.length - 1) {
        opts.separator.call(this);
      }

      if (opts.statement) this._printNewline(false, node, parent, newlineOpts);
    }

    if (opts.indent) this.dedent();
  }

  printAndIndentOnComments(node, parent) {
    const indent = node.leadingComments && node.leadingComments.length > 0;
    if (indent) this.indent();
    this.print(node, parent);
    if (indent) this.dedent();
  }

  printBlock(parent) {
    const node = parent.body;

    if (!isEmptyStatement(node)) {
      this.space();
    }

    this.print(node, parent);
  }

  _printTrailingComments(node) {
    this._printComments(this._getComments(false, node));
  }

  _printLeadingComments(node) {
    this._printComments(this._getComments(true, node), true);
  }

  printInnerComments(node, indent = true) {
    var _node$innerComments;

    if (!((_node$innerComments = node.innerComments) != null && _node$innerComments.length)) return;
    if (indent) this.indent();

    this._printComments(node.innerComments);

    if (indent) this.dedent();
  }

  printSequence(nodes, parent, opts = {}) {
    opts.statement = true;
    return this.printJoin(nodes, parent, opts);
  }

  printList(items, parent, opts = {}) {
    if (opts.separator == null) {
      opts.separator = commaSeparator;
    }

    return this.printJoin(items, parent, opts);
  }

  _printNewline(leading, node, parent, opts) {
    if (this.format.retainLines || this.format.compact) return;

    if (this.format.concise) {
      this.space();
      return;
    }

    let lines = 0;

    if (this._buf.hasContent()) {
      if (!leading) lines++;
      if (opts.addNewlines) lines += opts.addNewlines(leading, node) || 0;
      const needs = leading ? needsWhitespaceBefore : needsWhitespaceAfter;
      if (needs(node, parent)) lines++;
    }

    this.newline(Math.min(2, lines));
  }

  _getComments(leading, node) {
    return node && (leading ? node.leadingComments : node.trailingComments) || [];
  }

  _printComment(comment, skipNewLines) {
    if (!this.format.shouldPrintComment(comment.value)) return;
    if (comment.ignore) return;
    if (this._printedComments.has(comment)) return;

    this._printedComments.add(comment);

    const isBlockComment = comment.type === "CommentBlock";
    const printNewLines = isBlockComment && !skipNewLines && !this._noLineTerminator;
    if (printNewLines && this._buf.hasContent()) this.newline(1);
    const lastCharCode = this.getLastChar();

    if (lastCharCode !== 91 && lastCharCode !== 123) {
      this.space();
    }

    let val = !isBlockComment && !this._noLineTerminator ? `//${comment.value}\n` : `/*${comment.value}*/`;

    if (isBlockComment && this.format.indent.adjustMultilineComment) {
      var _comment$loc;

      const offset = (_comment$loc = comment.loc) == null ? void 0 : _comment$loc.start.column;

      if (offset) {
        const newlineRegex = new RegExp("\\n\\s{1," + offset + "}", "g");
        val = val.replace(newlineRegex, "\n");
      }

      const indentSize = Math.max(this._getIndent().length, this.format.retainLines ? 0 : this._buf.getCurrentColumn());
      val = val.replace(/\n(?!$)/g, `\n${" ".repeat(indentSize)}`);
    }

    if (this.endsWith(47)) this._space();
    this.withSource("start", comment.loc, () => {
      this._append(val);
    });
    if (printNewLines) this.newline(1);
  }

  _printComments(comments, inlinePureAnnotation) {
    if (!(comments != null && comments.length)) return;

    if (inlinePureAnnotation && comments.length === 1 && PURE_ANNOTATION_RE.test(comments[0].value)) {
      this._printComment(comments[0], this._buf.hasContent() && !this.endsWith(10));
    } else {
      for (const comment of comments) {
        this._printComment(comment);
      }
    }
  }

  printAssertions(node) {
    var _node$assertions;

    if ((_node$assertions = node.assertions) != null && _node$assertions.length) {
      this.space();
      this.word("assert");
      this.space();
      this.token("{");
      this.space();
      this.printList(node.assertions, node);
      this.space();
      this.token("}");
    }
  }

}

Object.assign(Printer.prototype, generatorFunctions);
{
  Printer.prototype.Noop = function Noop() {};
}
var _default = Printer;
exports.default = _default;

function commaSeparator() {
  this.token(",");
  this.space();
}