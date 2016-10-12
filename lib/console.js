'use strict';

const util = require('util');

class Console {
  constructor(stdout, stderr) {
    if (!stdout || typeof stdout.write !== 'function')
      throw new TypeError('Console expects a writable stream instance');

    if (!stderr)
      stderr = stdout;
    else if (typeof stderr.write !== 'function')
      throw new TypeError('Console expects writable stream instances');

    this._stdout = stdout;
    this._stderr = stderr;
    this._times = new Map();

    const methods = [
      'log', 'info', 'warn',
      'error', 'dir', 'time',
      'timeEnd', 'trace', 'assert'
    ];

    methods.forEach((method) => {
      this[method] = this[method].bind(this);
    });
  }

  // As of v8 5.0.71.32, the combination of rest param, template string
  // and .apply(null, args) benchmarks consistently faster than using
  // the spread operator when calling util.format.
  log(...args) {
    this._stdout.write(`${util.format.apply(null, args)}\n`);
  }

  info(...args) {
    this.log.apply(this, args);
  }

  warn(...args) {
    this._stderr.write(`${util.format.apply(null, args)}\n`);
  }

  error(...args) {
    this.warn.apply(this, args);
  }

  dir(object, options) {
    options = Object.assign({customInspect: false}, options);
    this._stdout.write(`${util.inspect(object, options)}\n`);
  }

  time(label) {
    this._times.set(label, process.hrtime());
  }

  timeEnd(label) {
    const time = this._times.get(label);
    if (!time) {
      process.emitWarning(`No such label '${label}' for console.timeEnd()`);
      return;
    }
    const duration = process.hrtime(time);
    const ms = duration[0] * 1000 + duration[1] / 1e6;
    this.log('%s: %sms', label, ms.toFixed(3));
    this._times.delete(label);
  }

  trace(...args) {
    // TODO probably can to do this better with V8's debug object once that is
    // exposed.
    var err = new Error();
    err.name = 'Trace';
    err.message = util.format.apply(null, args);
    Error.captureStackTrace(err, this.trace);
    this.error(err.stack);
  }

  assert(expression, ...args) {
    if (!expression)
      require('assert').ok(false, util.format.apply(null, args));
  }
}

module.exports = new Console(process.stdout, process.stderr);
module.exports.Console = Console;
