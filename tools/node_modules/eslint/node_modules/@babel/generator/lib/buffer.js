"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

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
    this._sourcePosition = {
      identifierName: undefined,
      line: undefined,
      column: undefined,
      filename: undefined
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
    const position = this._position;
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
      position.column += len;
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
      position.line++;
      position.column = 0;
      last = i + 1;

      if (last < len) {
        this._mark(++line, 0, identifierName, filename);
      }

      i = str.indexOf("\n", last);
    }

    position.column += len - last;
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

  getNewlineCount() {
    const queueCursor = this._queueCursor;
    let count = 0;
    if (queueCursor === 0) return this._last === 10 ? 1 : 0;

    for (let i = queueCursor - 1; i >= 0; i--) {
      if (this._queue[i].char !== 10) {
        break;
      }

      count++;
    }

    return count === queueCursor && this._last === 10 ? count + 1 : count;
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
  }

  source(prop, loc) {
    if (!this._map) return;

    this._normalizePosition(prop, loc, 0, 0);
  }

  sourceWithOffset(prop, loc, lineOffset, columnOffset) {
    if (!this._map) return;

    this._normalizePosition(prop, loc, lineOffset, columnOffset);
  }

  withSource(prop, loc, cb) {
    if (!this._map) return cb();
    this.source(prop, loc);
    cb();
  }

  _normalizePosition(prop, loc, lineOffset, columnOffset) {
    const pos = loc[prop];
    const target = this._sourcePosition;
    target.identifierName = prop === "start" && loc.identifierName || undefined;

    if (pos) {
      target.line = pos.line + lineOffset;
      target.column = pos.column + columnOffset;
      target.filename = loc.filename;
    }
  }

  getCurrentColumn() {
    const queue = this._queue;
    const queueCursor = this._queueCursor;
    let lastIndex = -1;
    let len = 0;

    for (let i = 0; i < queueCursor; i++) {
      const item = queue[i];

      if (item.char === 10) {
        lastIndex = len;
      }

      len += item.repeat;
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

//# sourceMappingURL=buffer.js.map
