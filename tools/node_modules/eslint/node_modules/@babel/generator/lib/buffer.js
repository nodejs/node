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

class Buffer {
  constructor(map) {
    this._map = null;
    this._buf = "";
    this._str = "";
    this._appendCount = 0;
    this._last = 0;
    this._queue = [];
    this._queueCursor = 0;
    this._position = {
      line: 1,
      column: 0
    };
    this._sourcePosition = SourcePos();
    this._disallowedPop = {
      identifierName: undefined,
      line: undefined,
      column: undefined,
      filename: undefined,
      objectReusable: true
    };
    this._map = map;

    this._allocQueue();
  }

  _allocQueue() {
    const queue = this._queue;

    for (let i = 0; i < 16; i++) {
      queue.push({
        char: 0,
        repeat: 1,
        line: undefined,
        column: undefined,
        identifierName: undefined,
        filename: ""
      });
    }
  }

  _pushQueue(char, repeat, line, column, identifierName, filename) {
    const cursor = this._queueCursor;

    if (cursor === this._queue.length) {
      this._allocQueue();
    }

    const item = this._queue[cursor];
    item.char = char;
    item.repeat = repeat;
    item.line = line;
    item.column = column;
    item.identifierName = identifierName;
    item.filename = filename;
    this._queueCursor++;
  }

  _popQueue() {
    if (this._queueCursor === 0) {
      throw new Error("Cannot pop from empty queue");
    }

    return this._queue[--this._queueCursor];
  }

  get() {
    this._flush();

    const map = this._map;
    const result = {
      code: (this._buf + this._str).trimRight(),
      decodedMap: map == null ? void 0 : map.getDecoded(),

      get map() {
        const resultMap = map ? map.get() : null;
        result.map = resultMap;
        return resultMap;
      },

      set map(value) {
        Object.defineProperty(result, "map", {
          value,
          writable: true
        });
      },

      get rawMappings() {
        const mappings = map == null ? void 0 : map.getRawMappings();
        result.rawMappings = mappings;
        return mappings;
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

  append(str, maybeNewline) {
    this._flush();

    this._append(str, this._sourcePosition, maybeNewline);
  }

  appendChar(char) {
    this._flush();

    this._appendChar(char, 1, this._sourcePosition);
  }

  queue(char) {
    if (char === 10) {
      while (this._queueCursor !== 0) {
        const char = this._queue[this._queueCursor - 1].char;

        if (char !== 32 && char !== 9) {
          break;
        }

        this._queueCursor--;
      }
    }

    const sourcePosition = this._sourcePosition;

    this._pushQueue(char, 1, sourcePosition.line, sourcePosition.column, sourcePosition.identifierName, sourcePosition.filename);
  }

  queueIndentation(char, repeat) {
    this._pushQueue(char, repeat, undefined, undefined, undefined, undefined);
  }

  _flush() {
    const queueCursor = this._queueCursor;
    const queue = this._queue;

    for (let i = 0; i < queueCursor; i++) {
      const item = queue[i];

      this._appendChar(item.char, item.repeat, item);
    }

    this._queueCursor = 0;
  }

  _appendChar(char, repeat, sourcePos) {
    this._last = char;
    this._str += repeat > 1 ? String.fromCharCode(char).repeat(repeat) : String.fromCharCode(char);

    if (char !== 10) {
      this._mark(sourcePos.line, sourcePos.column, sourcePos.identifierName, sourcePos.filename);

      this._position.column += repeat;
    } else {
      this._position.line++;
      this._position.column = 0;
    }
  }

  _append(str, sourcePos, maybeNewline) {
    const len = str.length;
    this._last = str.charCodeAt(len - 1);

    if (++this._appendCount > 4096) {
      +this._str;
      this._buf += this._str;
      this._str = str;
      this._appendCount = 0;
    } else {
      this._str += str;
    }

    if (!maybeNewline && !this._map) {
      this._position.column += len;
      return;
    }

    const {
      column,
      identifierName,
      filename
    } = sourcePos;
    let line = sourcePos.line;
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
    const queueCursor = this._queueCursor;

    if (queueCursor !== 0 && this._queue[queueCursor - 1].char === 10) {
      this._queueCursor--;
    }
  }

  removeLastSemicolon() {
    const queueCursor = this._queueCursor;

    if (queueCursor !== 0 && this._queue[queueCursor - 1].char === 59) {
      this._queueCursor--;
    }
  }

  getLastChar() {
    const queueCursor = this._queueCursor;
    return queueCursor !== 0 ? this._queue[queueCursor - 1].char : this._last;
  }

  endsWithCharAndNewline() {
    const queue = this._queue;
    const queueCursor = this._queueCursor;

    if (queueCursor !== 0) {
      const lastCp = queue[queueCursor - 1].char;
      if (lastCp !== 10) return;

      if (queueCursor > 1) {
        return queue[queueCursor - 2].char;
      } else {
        return this._last;
      }
    }
  }

  hasContent() {
    return this._queueCursor !== 0 || !!this._last;
  }

  exactSource(loc, cb) {
    if (!this._map) return cb();
    this.source("start", loc);
    cb();
    this.source("end", loc);

    this._disallowPop("start", loc);
  }

  source(prop, loc) {
    if (!loc) return;

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

    if (this._disallowedPop.objectReusable || this._disallowedPop.line !== originalLine || this._disallowedPop.column !== originalColumn || this._disallowedPop.filename !== originalFilename) {
      this._sourcePosition.line = originalLine;
      this._sourcePosition.column = originalColumn;
      this._sourcePosition.filename = originalFilename;
      this._sourcePosition.identifierName = originalIdentifierName;
      this._disallowedPop.objectReusable = true;
    }
  }

  _disallowPop(prop, loc) {
    if (!loc) return;
    const disallowedPop = this._disallowedPop;

    this._normalizePosition(prop, loc, disallowedPop);

    disallowedPop.objectReusable = false;
  }

  _normalizePosition(prop, loc, targetObj) {
    const pos = loc[prop];
    targetObj.identifierName = prop === "start" && loc.identifierName || undefined;

    if (pos) {
      targetObj.line = pos.line;
      targetObj.column = pos.column;
      targetObj.filename = loc.filename;
    } else {
      targetObj.line = null;
      targetObj.column = null;
      targetObj.filename = null;
    }
  }

  getCurrentColumn() {
    const queue = this._queue;
    let lastIndex = -1;
    let len = 0;

    for (let i = 0; i < this._queueCursor; i++) {
      const item = queue[i];

      if (item.char === 10) {
        lastIndex = i;
        len += item.repeat;
      }
    }

    return lastIndex === -1 ? this._position.column + len : len - 1 - lastIndex;
  }

  getCurrentLine() {
    let count = 0;
    const queue = this._queue;

    for (let i = 0; i < this._queueCursor; i++) {
      if (queue[i].char === 10) {
        count++;
      }
    }

    return this._position.line + count;
  }

}

exports.default = Buffer;