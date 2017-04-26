'use strict';

const util = require('util');
const kCounts = Symbol('counts');

function Console(stdout, stderr) {
  if (!(this instanceof Console)) {
    return new Console(stdout, stderr);
  }
  if (!stdout || typeof stdout.write !== 'function') {
    throw new TypeError('Console expects a writable stream instance');
  }
  if (!stderr) {
    stderr = stdout;
  } else if (typeof stderr.write !== 'function') {
    throw new TypeError('Console expects writable stream instances');
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
  prop.value = new Map();
  Object.defineProperty(this, '_times', prop);

  this[kCounts] = new Map();

  // bind the prototype functions to this Console instance
  var keys = Object.keys(Console.prototype);
  for (var v = 0; v < keys.length; v++) {
    var k = keys[v];
    this[k] = this[k].bind(this);
  }
}


// As of v8 5.0.71.32, the combination of rest param, template string
// and .apply(null, args) benchmarks consistently faster than using
// the spread operator when calling util.format.
Console.prototype.log = function(...args) {
  this._stdout.write(`${util.format.apply(null, args)}\n`);
};


Console.prototype.info = Console.prototype.log;


Console.prototype.warn = function(...args) {
  this._stderr.write(`${util.format.apply(null, args)}\n`);
};


Console.prototype.error = Console.prototype.warn;


Console.prototype.dir = function(object, options) {
  options = Object.assign({customInspect: false}, options);
  this._stdout.write(`${util.inspect(object, options)}\n`);
};


Console.prototype.time = function(label) {
  this._times.set(label, process.hrtime());
};


Console.prototype.timeEnd = function(label) {
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
  // TODO probably can to do this better with V8's debug object once that is
  // exposed.
  var err = new Error();
  err.name = 'Trace';
  err.message = util.format.apply(null, args);
  Error.captureStackTrace(err, trace);
  this.error(err.stack);
};


Console.prototype.assert = function(expression, ...args) {
  if (!expression) {
    require('assert').ok(false, util.format.apply(null, args));
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

module.exports = new Console(process.stdout, process.stderr);
module.exports.Console = Console;
