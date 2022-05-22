"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

function SourcePos() {
  return {
    identifierName: undefined,
    line: undefined,
    column: undefined,
    filename: undefined
  };
}

const SPACES_RE = /^[ \t]+$/;

class Buffer {
  constructor(map) {
    this._map = null;
    this._buf = "";
    this._last = 0;
    this._queue = [];
    this._position = {
      line: 1,
      column: 0
    };
    this._sourcePosition = SourcePos();
    this._disallowedPop = null;
    this._map = map;
  }

  get() {
    this._flush();

    const map = this._map;
    const result = {
      code: this._buf.trimRight(),
      decodedMap: map == null ? void 0 : map.getDecoded(),

      get map() {
        return result.map = map ? map.get() : null;
      },

      set map(value) {
        Object.defineProperty(result, "map", {
          value,
          writable: true
        });
      },

      get rawMappings() {
        return result.rawMappings = map == null ? void 0 : map.getRawMappings();
      },

      set rawMappings(value) {
        Object.defineProperty(result, "rawMappings", {
          value,
          writable: true
        });
      }

    };
    return result;
  }

  append(str) {
    this._flush();

    const {
      line,
      column,
      filename,
      identifierName
    } = this._sourcePosition;

    this._append(str, line, column, identifierName, filename);
  }

  queue(str) {
    if (str === "\n") {
      while (this._queue.length > 0 && SPACES_RE.test(this._queue[0][0])) {
        this._queue.shift();
      }
    }

    const {
      line,
      column,
      filename,
      identifierName
    } = this._sourcePosition;

    this._queue.unshift([str, line, column, identifierName, filename]);
  }

  queueIndentation(str) {
    this._queue.unshift([str, undefined, undefined, undefined, undefined]);
  }

  _flush() {
    let item;

    while (item = this._queue.pop()) {
      this._append(...item);
    }
  }

  _append(str, line, column, identifierName, filename) {
    this._buf += str;
    this._last = str.charCodeAt(str.length - 1);
    let i = str.indexOf("\n");
    let last = 0;

    if (i !== 0) {
      this._mark(line, column, identifierName, filename);
    }

    while (i !== -1) {
      this._position.line++;
      this._position.column = 0;
      last = i + 1;

      if (last < str.length) {
        this._mark(++line, 0, identifierName, filename);
      }

      i = str.indexOf("\n", last);
    }

    this._position.column += str.length - last;
  }

  _mark(line, column, identifierName, filename) {
    var _this$_map;

    (_this$_map = this._map) == null ? void 0 : _this$_map.mark(this._position, line, column, identifierName, filename);
  }

  removeTrailingNewline() {
    if (this._queue.length > 0 && this._queue[0][0] === "\n") {
      this._queue.shift();
    }
  }

  removeLastSemicolon() {
    if (this._queue.length > 0 && this._queue[0][0] === ";") {
      this._queue.shift();
    }
  }

  getLastChar() {
    let last;

    if (this._queue.length > 0) {
      const str = this._queue[0][0];
      last = str.charCodeAt(0);
    } else {
      last = this._last;
    }

    return last;
  }

  endsWithCharAndNewline() {
    const queue = this._queue;

    if (queue.length > 0) {
      const last = queue[0][0];
      const lastCp = last.charCodeAt(0);
      if (lastCp !== 10) return;

      if (queue.length > 1) {
        const secondLast = queue[1][0];
        return secondLast.charCodeAt(0);
      } else {
        return this._last;
      }
    }
  }

  hasContent() {
    return this._queue.length > 0 || !!this._last;
  }

  exactSource(loc, cb) {
    this.source("start", loc);
    cb();
    this.source("end", loc);

    this._disallowPop("start", loc);
  }

  source(prop, loc) {
    if (prop && !loc) return;

    this._normalizePosition(prop, loc, this._sourcePosition);
  }

  withSource(prop, loc, cb) {
    if (!this._map) return cb();
    const originalLine = this._sourcePosition.line;
    const originalColumn = this._sourcePosition.column;
    const originalFilename = this._sourcePosition.filename;
    const originalIdentifierName = this._sourcePosition.identifierName;
    this.source(prop, loc);
    cb();

    if (!this._disallowedPop || this._disallowedPop.line !== originalLine || this._disallowedPop.column !== originalColumn || this._disallowedPop.filename !== originalFilename) {
      this._sourcePosition.line = originalLine;
      this._sourcePosition.column = originalColumn;
      this._sourcePosition.filename = originalFilename;
      this._sourcePosition.identifierName = originalIdentifierName;
      this._disallowedPop = null;
    }
  }

  _disallowPop(prop, loc) {
    if (prop && !loc) return;
    this._disallowedPop = this._normalizePosition(prop, loc, SourcePos());
  }

  _normalizePosition(prop, loc, targetObj) {
    const pos = loc ? loc[prop] : null;
    targetObj.identifierName = prop === "start" && (loc == null ? void 0 : loc.identifierName) || undefined;
    targetObj.line = pos == null ? void 0 : pos.line;
    targetObj.column = pos == null ? void 0 : pos.column;
    targetObj.filename = loc == null ? void 0 : loc.filename;
    return targetObj;
  }

  getCurrentColumn() {
    const extra = this._queue.reduce((acc, item) => item[0] + acc, "");

    const lastIndex = extra.lastIndexOf("\n");
    return lastIndex === -1 ? this._position.column + extra.length : extra.length - 1 - lastIndex;
  }

  getCurrentLine() {
    const extra = this._queue.reduce((acc, item) => item[0] + acc, "");

    let count = 0;

    for (let i = 0; i < extra.length; i++) {
      if (extra[i] === "\n") count++;
    }

    return this._position.line + count;
  }

}

exports.default = Buffer;