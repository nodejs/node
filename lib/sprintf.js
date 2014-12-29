// Copyright (c) 2014, Ben Noordhuis <info@bnoordhuis.nl>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

'use strict';

var L = require('_linklist');
var inspect = require('util').inspect;
var quickeval = process.binding('contextify').quickeval;

function LRU(maxsize) {
  this.dict = Object.create(null);
  this.list = L.init({});
  this.size = 0;
  this.maxsize = maxsize;
}

LRU.prototype.find = function(key) {
  var val = this.dict[key];

  // Move to front of the list unless it's already the first node.
  if (val && val !== L.peek(this.list))
    L.append(this.list, L.remove(val));

  return val;
};

LRU.prototype.insert = function(key, val) {
  while (this.size >= this.maxsize) {
    delete this.dict[L.shift(this.list).key];
    this.size -= 1;
  }

  var val = L.append(this.list, {key: key});
  this.dict[key] = val;
  this.size += 1;

  return val;
};

function format(fmt /*, ... */) {
  return vsprintf(fmt, arguments, compat);
}

function sprintf(fmt /*, ... */) {
  return vsprintf(fmt, arguments, extended);
}

function vsprintf(fmt, argv, compile) {
  if (typeof(fmt) !== 'string') {
    if (compile === compat)
      return concat(argv);
    fmt = '' + fmt;
  }

  var lru = compile.lru;
  var cached = lru.find(fmt);

  if (cached === undefined) {
    cached = lru.insert(fmt);
    cached.fun = compile(fmt);
  }

  return cached.fun.apply(null, argv);
}

function concat(argv) {
  var argc = argv.length;
  var objects = new Array(argc);

  for (var i = 0; i < argc; i += 1)
    objects[i] = inspect(argv[i]);

  return objects.join(' ');
}

// util.format() compatibility mode.
function compat(fmt) {
  var index = 0;
  var argc = 0;
  var body = '';

  for (;;) {
    var start = index;
    index = fmt.indexOf('%', index);
    var end = index === -1 ? fmt.length : index;

    if (start !== end) {
      body += ' s += "' + untaint(fmt.slice(start, end)) + '";';
    }

    if (index === -1)
      break;

    index += 1;

    var c = fmt[index];
    switch (c) {
      case 'd':
        // %d is really %f; it coerces the argument to a number
        // before turning it into a string.
        body += ' s += argc > ' + (argc + 1) + ' ? +a' + argc + ' : "%d";';
        argc += 1;
        index += 1;
        continue;

      case 'j':
        body += ' if (argc > ' + (argc + 1) + ')';
        body += ' try { s += JSON.stringify(a' + argc + '); }';
        body += ' catch (e) { s += "[Circular]"; }';
        body += ' else s += "%j";';
        argc += 1;
        index += 1;
        continue;

      case 's':
        body += ' s += argc > ' + (argc + 1) + ' ? a' + argc + ' : "%s";';
        argc += 1;
        index += 1;
        continue;

      case '"':
        c = '\\"';
        // Fall through.

      default:
        body += ' s += "' + c + '";';
        index += 1;
        continue;
    }
  }

  // The dummy argument lets format() and sprintf() call the formatter
  // with .apply() without having to unshift the format argument first.
  var source = 'return function(dummy';

  for (var i = 0; i < argc; i += 1)
    source += ', a' + i;

  source += ') { var argc = arguments.length, s = "";' + body;
  source += ' for (var i = ' + argc + ' + 1; i < arguments.length; i += 1) {';
  source += ' var arg = arguments[i];';
  source += ' if (arg === null || typeof(arg) !== "object") s += " " + arg;';
  source += ' else s += " " + inspect(arg);';
  source += ' }';
  source += ' return s;';
  source += ' };';

  return Function('inspect', source)(inspect);
}

function untaint(s) {
  return s.replace(/[\\"]|[^\x20-\x7F]/g, function(c) {
    switch (c) {
      case '\t': return '\\t';
      case '\n': return '\\n';
      case '\f': return '\\f';
      case '\r': return '\\r';
      case '"': return '\\"';
      case '\\': return '\\\\';
    }
    return '\\u' + ('0000' + c.charCodeAt(0).toString(16)).slice(-4);
  });
}

compat.lru = new LRU(64);

function extended(fmt) {
  throw Error('unimplemented');
}

extended.lru = new LRU(64);

// Hidden export for lib/util.js
Object.defineProperty(sprintf, '_format', {value: format});

module.exports = sprintf;
