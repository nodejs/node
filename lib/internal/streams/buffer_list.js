'use strict';

const { Buffer } = require('buffer');
const { inspect } = require('util');

function copyBuffer(src, target, offset) {
  Buffer.prototype.copy.call(src, target, offset);
}

module.exports = class BufferList {
  constructor() {
    this.head = null;
    this.tail = null;
    this.length = 0;
  }

  push(v) {
    const entry = { data: v, next: null };
    if (this.length > 0)
      this.tail.next = entry;
    else
      this.head = entry;
    this.tail = entry;
    ++this.length;
  }

  unshift(v) {
    const entry = { data: v, next: this.head };
    if (this.length === 0)
      this.tail = entry;
    this.head = entry;
    ++this.length;
  }

  shift() {
    if (this.length === 0)
      return;
    const ret = this.head.data;
    if (this.length === 1)
      this.head = this.tail = null;
    else
      this.head = this.head.next;
    --this.length;
    return ret;
  }

  clear() {
    this.head = this.tail = null;
    this.length = 0;
  }

  join(s) {
    if (this.length === 0)
      return '';
    var p = this.head;
    var ret = '' + p.data;
    while (p = p.next)
      ret += s + p.data;
    return ret;
  }

  concat(n) {
    if (this.length === 0)
      return Buffer.alloc(0);
    const ret = Buffer.allocUnsafe(n >>> 0);
    var p = this.head;
    var i = 0;
    while (p) {
      copyBuffer(p.data, ret, i);
      i += p.data.length;
      p = p.next;
    }
    return ret;
  }

  // Consumes a specified amount of bytes or characters from the buffered data.
  consume(n, hasStrings) {
    var ret;
    if (n < this.head.data.length) {
      // `slice` is the same for buffers and strings.
      ret = this.head.data.slice(0, n);
      this.head.data = this.head.data.slice(n);
    } else if (n === this.head.data.length) {
      // First chunk is a perfect match.
      ret = this.shift();
    } else {
      // Result spans more than one buffer.
      ret = hasStrings ? this._getString(n) : this._getBuffer(n);
    }
    return ret;
  }

  first() {
    return this.head.data;
  }

  // Consumes a specified amount of characters from the buffered data.
  _getString(n) {
    var p = this.head;
    var c = 1;
    var ret = p.data;
    n -= ret.length;
    while (p = p.next) {
      const str = p.data;
      const nb = (n > str.length ? str.length : n);
      if (nb === str.length)
        ret += str;
      else
        ret += str.slice(0, n);
      n -= nb;
      if (n === 0) {
        if (nb === str.length) {
          ++c;
          if (p.next)
            this.head = p.next;
          else
            this.head = this.tail = null;
        } else {
          this.head = p;
          p.data = str.slice(nb);
        }
        break;
      }
      ++c;
    }
    this.length -= c;
    return ret;
  }

  // Consumes a specified amount of bytes from the buffered data.
  _getBuffer(n) {
    const ret = Buffer.allocUnsafe(n);
    var p = this.head;
    var c = 1;
    p.data.copy(ret);
    n -= p.data.length;
    while (p = p.next) {
      const buf = p.data;
      const nb = (n > buf.length ? buf.length : n);
      buf.copy(ret, ret.length - n, 0, nb);
      n -= nb;
      if (n === 0) {
        if (nb === buf.length) {
          ++c;
          if (p.next)
            this.head = p.next;
          else
            this.head = this.tail = null;
        } else {
          this.head = p;
          p.data = buf.slice(nb);
        }
        break;
      }
      ++c;
    }
    this.length -= c;
    return ret;
  }

  [inspect.custom]() {
    const obj = inspect({ length: this.length });
    return `${this.constructor.name} ${obj}`;
  }
};
