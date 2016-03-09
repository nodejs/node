'use strict';

const util = require('util');

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

  // bind the prototype functions to this Console instance
  var keys = Object.keys(Console.prototype);
  for (var v = 0; v < keys.length; v++) {
    var k = keys[v];
    this[k] = this[k].bind(this);
  }
}

Console.prototype.log = function() {
  this._stdout.write(util.format.apply(null, arguments) + '\n');
};


Console.prototype.info = Console.prototype.log;


Console.prototype.warn = function() {
  this._stderr.write(util.format.apply(null, arguments) + '\n');
};


Console.prototype.error = Console.prototype.warn;


Console.prototype.dir = function(object, options) {
  this._stdout.write(util.inspect(object, util._extend({
    customInspect: false
  }, options)) + '\n');
};


Console.prototype.time = function(label) {
  this._times.set(label, Date.now());
};


Console.prototype.timeEnd = function(label) {
  var time = this._times.get(label);
  if (!time) {
    throw new Error('No such label: ' + label);
  }
  var duration = Date.now() - time;
  this.log('%s: %dms', label, duration);
};


Console.prototype.trace = function trace() {
  // TODO probably can to do this better with V8's debug object once that is
  // exposed.
  var err = new Error();
  err.name = 'Trace';
  err.message = util.format.apply(null, arguments);
  Error.captureStackTrace(err, trace);
  this.error(err.stack);
};


Console.prototype.assert = function(expression) {
  if (!expression) {
    const argsLen = arguments.length || 1;
    const arr = new Array(argsLen - 1);
    for (var i = 1; i < argsLen; i++)
      arr[i - 1] = arguments[i];
    require('assert').ok(false, util.format.apply(null, arr));
  }
};


module.exports = new Console(process.stdout, process.stderr);
module.exports.Console = Console;
