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

const {
  isStackOverflowError,
  codes: {
    ERR_CONSOLE_WRITABLE_STREAM,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
  },
} = require('internal/errors');
const { previewEntries } = process.binding('util');
const { Buffer: { isBuffer } } = require('buffer');
const util = require('util');
const {
  isTypedArray, isSet, isMap, isSetIterator, isMapIterator,
} = util.types;
const kCounts = Symbol('counts');

const {
  keys: ObjectKeys,
  values: ObjectValues,
} = Object;
const hasOwnProperty = Function.call.bind(Object.prototype.hasOwnProperty);

const {
  isArray: ArrayIsArray,
  from: ArrayFrom,
} = Array;

// Lazy loaded for startup performance.
let cliTable;

// Track amount of indentation required via `console.group()`.
const kGroupIndent = Symbol('kGroupIndent');

const kFormatForStderr = Symbol('kFormatForStderr');
const kFormatForStdout = Symbol('kFormatForStdout');
const kGetInspectOptions = Symbol('kGetInspectOptions');
const kColorMode = Symbol('kColorMode');

function Console(options /* or: stdout, stderr, ignoreErrors = true */) {
  if (!(this instanceof Console)) {
    return new Console(...arguments);
  }

  if (!options || typeof options.write === 'function') {
    options = {
      stdout: options,
      stderr: arguments[1],
      ignoreErrors: arguments[2]
    };
  }

  const {
    stdout,
    stderr = stdout,
    ignoreErrors = true,
    colorMode = 'auto'
  } = options;

  if (!stdout || typeof stdout.write !== 'function') {
    throw new ERR_CONSOLE_WRITABLE_STREAM('stdout');
  }
  if (!stderr || typeof stderr.write !== 'function') {
    throw new ERR_CONSOLE_WRITABLE_STREAM('stderr');
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

  if (typeof colorMode !== 'boolean' && colorMode !== 'auto')
    throw new ERR_INVALID_ARG_VALUE('colorMode', colorMode);

  this[kCounts] = new Map();
  this[kColorMode] = colorMode;

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
      // removed after the event, non-console.* writes won't be affected.
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
    // console is a debugging utility, so it swallowing errors is not desirable
    // even in edge cases such as low stack space.
    if (isStackOverflowError(e))
      throw e;
    // Sorry, there's no proper way to pass along the error here.
  } finally {
    stream.removeListener('error', noop);
  }
}

const kColorInspectOptions = { colors: true };
const kNoColorInspectOptions = {};
Console.prototype[kGetInspectOptions] = function(stream) {
  let color = this[kColorMode];
  if (color === 'auto') {
    color = stream.isTTY && (
      typeof stream.getColorDepth === 'function' ?
        stream.getColorDepth() > 2 : true);
  }

  return color ? kColorInspectOptions : kNoColorInspectOptions;
};

Console.prototype[kFormatForStdout] = function(args) {
  const opts = this[kGetInspectOptions](this._stdout);
  return util.formatWithOptions(opts, ...args);
};

Console.prototype[kFormatForStderr] = function(args) {
  const opts = this[kGetInspectOptions](this._stderr);
  return util.formatWithOptions(opts, ...args);
};

Console.prototype.log = function log(...args) {
  write(this._ignoreErrors,
        this._stdout,
        this[kFormatForStdout](args),
        this._stdoutErrorHandler,
        this[kGroupIndent]);
};
Console.prototype.debug = Console.prototype.log;
Console.prototype.info = Console.prototype.log;
Console.prototype.dirxml = Console.prototype.log;

Console.prototype.warn = function warn(...args) {
  write(this._ignoreErrors,
        this._stderr,
        this[kFormatForStderr](args),
        this._stderrErrorHandler,
        this[kGroupIndent]);
};
Console.prototype.error = Console.prototype.warn;

Console.prototype.dir = function dir(object, options) {
  options = Object.assign({
    customInspect: false
  }, this[kGetInspectOptions](this._stdout), options);
  write(this._ignoreErrors,
        this._stdout,
        util.inspect(object, options),
        this._stdoutErrorHandler,
        this[kGroupIndent]);
};

Console.prototype.time = function time(label = 'default') {
  // Coerces everything other than Symbol to a string
  label = `${label}`;
  if (this._times.has(label)) {
    process.emitWarning(`Label '${label}' already exists for console.time()`);
    return;
  }
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
    message: this[kFormatForStderr](args)
  };
  Error.captureStackTrace(err, trace);
  this.error(err.stack);
};

Console.prototype.assert = function assert(expression, ...args) {
  if (!expression) {
    args[0] = `Assertion failed${args.length === 0 ? '' : `: ${args[0]}`}`;
    this.warn(this[kFormatForStderr](args));
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

const keyKey = 'Key';
const valuesKey = 'Values';
const indexKey = '(index)';
const iterKey = '(iteration index)';


const isArray = (v) => ArrayIsArray(v) || isTypedArray(v) || isBuffer(v);

// https://console.spec.whatwg.org/#table
Console.prototype.table = function(tabularData, properties) {
  if (properties !== undefined && !ArrayIsArray(properties))
    throw new ERR_INVALID_ARG_TYPE('properties', 'Array', properties);

  if (tabularData == null || typeof tabularData !== 'object')
    return this.log(tabularData);

  if (cliTable === undefined) cliTable = require('internal/cli_table');
  const final = (k, v) => this.log(cliTable(k, v));

  const inspect = (v) => {
    const opt = { depth: 0, maxArrayLength: 3 };
    if (v !== null && typeof v === 'object' &&
      !isArray(v) && ObjectKeys(v).length > 2)
      opt.depth = -1;
    Object.assign(opt, this[kGetInspectOptions](this._stdout));
    return util.inspect(v, opt);
  };
  const getIndexArray = (length) => ArrayFrom({ length }, (_, i) => inspect(i));

  const mapIter = isMapIterator(tabularData);
  if (mapIter)
    tabularData = previewEntries(tabularData);

  if (mapIter || isMap(tabularData)) {
    const keys = [];
    const values = [];
    let length = 0;
    for (const [k, v] of tabularData) {
      keys.push(inspect(k));
      values.push(inspect(v));
      length++;
    }
    return final([
      iterKey, keyKey, valuesKey
    ], [
      getIndexArray(length),
      keys,
      values,
    ]);
  }

  const setIter = isSetIterator(tabularData);
  if (setIter)
    tabularData = previewEntries(tabularData);

  const setlike = setIter || isSet(tabularData);
  if (setlike) {
    const values = [];
    let length = 0;
    for (const v of tabularData) {
      values.push(inspect(v));
      length++;
    }
    return final([setlike ? iterKey : indexKey, valuesKey], [
      getIndexArray(length),
      values,
    ]);
  }

  const map = {};
  let hasPrimitives = false;
  const valuesKeyArray = [];
  const indexKeyArray = ObjectKeys(tabularData);

  for (var i = 0; i < indexKeyArray.length; i++) {
    const item = tabularData[indexKeyArray[i]];
    const primitive = item === null ||
        (typeof item !== 'function' && typeof item !== 'object');
    if (properties === undefined && primitive) {
      hasPrimitives = true;
      valuesKeyArray[i] = inspect(item);
    } else {
      const keys = properties || ObjectKeys(item);
      for (const key of keys) {
        if (map[key] === undefined)
          map[key] = [];
        if ((primitive && properties) || !hasOwnProperty(item, key))
          map[key][i] = '';
        else
          map[key][i] = item == null ? item : inspect(item[key]);
      }
    }
  }

  const keys = ObjectKeys(map);
  const values = ObjectValues(map);
  if (hasPrimitives) {
    keys.push(valuesKey);
    values.push(valuesKeyArray);
  }
  keys.unshift(indexKey);
  values.unshift(indexKeyArray);

  return final(keys, values);
};

module.exports = new Console({
  stdout: process.stdout,
  stderr: process.stderr
});
module.exports.Console = Console;

function noop() {}
