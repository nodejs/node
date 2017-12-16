// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';

const errors = require('internal/errors');
const util = require('util');
const kCounts = Symbol('counts');

// Track amount of indentation required via `console.group()`.
const kGroupIndent = Symbol('groupIndent');

let MAX_STACK_MESSAGE;

function Console(stdout, stderr, ignoreErrors = true) {
  if (!(this instanceof Console)) {
    return new Console(stdout, stderr, ignoreErrors);
  }
  if (!stdout || typeof stdout.write !== 'function') {
    throw new errors.TypeError('ERR_CONSOLE_WRITABLE_STREAM', 'stdout');
  }
  if (!stderr) {
    stderr = stdout;
  } else if (typeof stderr.write !== 'function') {
    throw new errors.TypeError('ERR_CONSOLE_WRITABLE_STREAM', 'stderr');
  }

  var prop = {
    writable: true,
    enumerable: false,
    configurable: true
  };
  prop.value = stdout;
  Object.defineProperty(this, '_stdout', prop);
  prop.value = stderr;
  Object.defineProperty(this, '_stderr', prop);
  prop.value = Boolean(ignoreErrors);
  Object.defineProperty(this, '_ignoreErrors', prop);
  prop.value = new Map();
  Object.defineProperty(this, '_times', prop);
  prop.value = createWriteErrorHandler(stdout);
  Object.defineProperty(this, '_stdoutErrorHandler', prop);
  prop.value = createWriteErrorHandler(stderr);
  Object.defineProperty(this, '_stderrErrorHandler', prop);

  this[kCounts] = new Map();

  Object.defineProperty(this, kGroupIndent, { writable: true });
  this[kGroupIndent] = '';

  // bind the prototype functions to this Console instance
  var keys = Object.keys(Console.prototype);
  for (var v = 0; v < keys.length; v++) {
    var k = keys[v];
    this[k] = this[k].bind(this);
  }
}

// Make a function that can serve as the callback passed to `stream.write()`.
function createWriteErrorHandler(stream) {
  return (err) => {
    // This conditional evaluates to true if and only if there was an error
    // that was not already emitted (which happens when the _write callback
    // is invoked asynchronously).
    if (err !== null && !stream._writableState.errorEmitted) {
      // If there was an error, it will be emitted on `stream` as
      // an `error` event. Adding a `once` listener will keep that error
      // from becoming an uncaught exception, but since the handler is
      // removed after the event, non-console.* writes won’t be affected.
      // we are only adding noop if there is no one else listening for 'error'
      if (stream.listenerCount('error') === 0) {
        stream.on('error', noop);
      }
    }
  };
}

function write(ignoreErrors, stream, string, errorhandler, groupIndent) {
  if (groupIndent.length !== 0) {
    if (string.indexOf('\n') !== -1) {
      string = string.replace(/\n/g, `\n${groupIndent}`);
    }
    string = groupIndent + string;
  }
  string += '\n';

  if (ignoreErrors === false) return stream.write(string);

  // There may be an error occurring synchronously (e.g. for files or TTYs
  // on POSIX systems) or asynchronously (e.g. pipes on POSIX systems), so
  // handle both situations.
  try {
    // Add and later remove a noop error handler to catch synchronous errors.
    stream.once('error', noop);

    stream.write(string, errorhandler);
  } catch (e) {
    if (MAX_STACK_MESSAGE === undefined) {
      try {
        // eslint-disable-next-line no-unused-vars
        function a() { a(); }
      } catch (err) {
        MAX_STACK_MESSAGE = err.message;
      }
    }
    // console is a debugging utility, so it swallowing errors is not desirable
    // even in edge cases such as low stack space.
    if (e.message === MAX_STACK_MESSAGE && e.name === 'RangeError')
      throw e;
    // Sorry, there’s no proper way to pass along the error here.
  } finally {
    stream.removeListener('error', noop);
  }
}

Console.prototype.log = function log(...args) {
  write(this._ignoreErrors,
        this._stdout,
        // The performance of .apply and the spread operator seems on par in V8
        // 6.3 but the spread operator, unlike .apply(), pushes the elements
        // onto the stack. That is, it makes stack overflows more likely.
        util.format.apply(null, args),
        this._stdoutErrorHandler,
        this[kGroupIndent]);
};
Console.prototype.debug = Console.prototype.log;
Console.prototype.info = Console.prototype.log;
Console.prototype.dirxml = Console.prototype.log;

Console.prototype.warn = function warn(...args) {
  write(this._ignoreErrors,
        this._stderr,
        util.format.apply(null, args),
        this._stderrErrorHandler,
        this[kGroupIndent]);
};
Console.prototype.error = Console.prototype.warn;

Console.prototype.dir = function dir(object, options) {
  options = Object.assign({ customInspect: false }, options);
  write(this._ignoreErrors,
        this._stdout,
        util.inspect(object, options),
        this._stdoutErrorHandler,
        this[kGroupIndent]);
};

Console.prototype.time = function time(label = 'default') {
  // Coerces everything other than Symbol to a string
  label = `${label}`;
  this._times.set(label, process.hrtime());
};

Console.prototype.timeEnd = function timeEnd(label = 'default') {
  // Coerces everything other than Symbol to a string
  label = `${label}`;
  const time = this._times.get(label);
  if (!time) {
    process.emitWarning(`No such label '${label}' for console.timeEnd()`);
    return;
  }
  const duration = process.hrtime(time);
  const ms = duration[0] * 1000 + duration[1] / 1e6;
  this.log('%s: %sms', label, ms.toFixed(3));
  this._times.delete(label);
};

Console.prototype.trace = function trace(...args) {
  const err = {
    name: 'Trace',
    message: util.format.apply(null, args)
  };
  Error.captureStackTrace(err, trace);
  this.error(err.stack);
};

Console.prototype.assert = function assert(expression, ...args) {
  if (!expression) {
    args[0] = `Assertion failed${args.length === 0 ? '' : `: ${args[0]}`}`;
    this.warn(util.format.apply(null, args));
  }
};

// Defined by: https://console.spec.whatwg.org/#clear
Console.prototype.clear = function clear() {
  // It only makes sense to clear if _stdout is a TTY.
  // Otherwise, do nothing.
  if (this._stdout.isTTY) {
    // The require is here intentionally to avoid readline being
    // required too early when console is first loaded.
    const { cursorTo, clearScreenDown } = require('readline');
    cursorTo(this._stdout, 0, 0);
    clearScreenDown(this._stdout);
  }
};

// Defined by: https://console.spec.whatwg.org/#count
Console.prototype.count = function count(label = 'default') {
  // Ensures that label is a string, and only things that can be
  // coerced to strings. e.g. Symbol is not allowed
  label = `${label}`;
  const counts = this[kCounts];
  let count = counts.get(label);
  if (count === undefined)
    count = 1;
  else
    count++;
  counts.set(label, count);
  this.log(`${label}: ${count}`);
};

// Not yet defined by the https://console.spec.whatwg.org, but
// proposed to be added and currently implemented by Edge. Having
// the ability to reset counters is important to help prevent
// the counter from being a memory leak.
Console.prototype.countReset = function countReset(label = 'default') {
  const counts = this[kCounts];
  counts.delete(`${label}`);
};

Console.prototype.group = function group(...data) {
  if (data.length > 0) {
    this.log(...data);
  }
  this[kGroupIndent] += '  ';
};
Console.prototype.groupCollapsed = Console.prototype.group;

Console.prototype.groupEnd = function groupEnd() {
  this[kGroupIndent] =
    this[kGroupIndent].slice(0, this[kGroupIndent].length - 2);
};

module.exports = new Console(process.stdout, process.stderr);
module.exports.Console = Console;

function noop() {}
