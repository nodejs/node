'use strict';

const util = require('util');

class Console {
  constructor(stdout, stderr) {
    if (!stdout || typeof stdout.write !== 'function')
      throw new TypeError('Console expects a writable stream instance');

    const prop = {
      configurable: true,
      enumerable: false,
      value: stdout,
      writable: true
    };

    Object.defineProperty(this, '_stdout', prop);

    if (stderr)
      prop.value = stderr;

    Object.defineProperty(this, '_stderr', prop);
    prop.value = new Map();
    Object.defineProperty(this, '_times', prop);

    const keys = Object.getOwnPropertyNames(Console.prototype);
    for (let v = 1; v < keys.length; v++) {
      const k = keys[v];
      this[k] = this[k].bind(this);
    }
  }

  log() {
    this._stdout.write(util.format.apply(this, arguments) + '\n');
  }

  info() {
    this._stdout.write(util.format.apply(this, arguments) + '\n');
  }

  warn() {
    this._stderr.write(util.format.apply(this, arguments) + '\n');
  }

  error() {
    this._stderr.write(util.format.apply(this, arguments) + '\n');
  }

  dir(object, options) {
    this._stdout.write(util.inspect(object, util._extend({
      customInspect: false
    }, options)) + '\n');
  }

  time(label) {
    this._times.set(label, process.hrtime());
  }

  timeEnd(label) {
    const time = this._times.get(label);

    if (!time)
      throw new Error('No such label: ' + label);

    const duration = process.hrtime(time);
    const ms = duration[0] * 1000 + duration[1] / 1e6;
    this.log('%s: %sms', label, ms.toFixed(3));
    this._times.delete(label);
  }

  trace() {
    // TODO probably can do this better with V8's debug object once that is
    // exposed.
    const err = new Error();
    err.name = 'Trace';
    err.message = util.format.apply(this, arguments);
    Error.captureStackTrace(err, Console.trace);
    this.error(err.stack);
  }

  assert(expression) {
    if (!expression) {
      const arr = Array.prototype.slice.call(arguments, 1);
      require('assert').ok(false, util.format.apply(this, arr));
    }
  }

}

module.exports = new Console(process.stdout, process.stderr);
module.exports.Console = Console;
