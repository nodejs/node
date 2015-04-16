'use strict';

const util = require('util');

const LOG_FUNC_LOG = 2;
const LOG_FUNC_INFO = 3;
const LOG_FUNC_WARN = 4;
const LOG_FUNC_ERROR = 5;
const LOG_FUNC_DIR = 6;


function Console(stdout, stderr, logger) {
  if (!(this instanceof Console)) {
    return new Console(stdout, stderr);
  }
  if (!stdout || typeof stdout.write !== 'function') {
    throw new TypeError('Console expects a writable stream instance');
  }
  if (!stderr) {
    stderr = stdout;
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

  prop.value = logger || function(func, message) {
    // Default logging function.
    switch (func) {
      case LOG_FUNC_LOG:
      case LOG_FUNC_INFO:
      case LOG_FUNC_DIR:
        this._stdout.write(message + '\n');
        break;

      case LOG_FUNC_WARN:
      case LOG_FUNC_ERROR:
        this._stderr.write(message + '\n');
        break;
    }
  };
  Object.defineProperty(this, '_logger', prop);

  // bind the prototype functions to this Console instance
  var keys = Object.keys(Console.prototype);
  for (var v = 0; v < keys.length; v++) {
    var k = keys[v];
    this[k] = this[k].bind(this);
  }
}


Console.prototype.log = function() {
  this._logger(LOG_FUNC_LOG, util.format.apply(this, arguments));
};


Console.prototype.info = function() {
  this._logger(LOG_FUNC_INFO, util.format.apply(this, arguments));
};


Console.prototype.warn = function() {
  this._logger(LOG_FUNC_WARN, util.format.apply(this, arguments));
};


Console.prototype.error = function() {
  this._logger(LOG_FUNC_ERROR, util.format.apply(this, arguments));
};


Console.prototype.dir = function(object, options) {
  this._logger(LOG_FUNC_DIR, util.inspect(object, util._extend({
    customInspect: false
  }, options)));
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
  var err = new Error;
  err.name = 'Trace';
  err.message = util.format.apply(this, arguments);
  Error.captureStackTrace(err, trace);
  this.error(err.stack);
};


Console.prototype.assert = function(expression) {
  if (!expression) {
    var arr = Array.prototype.slice.call(arguments, 1);
    require('assert').ok(false, util.format.apply(this, arr));
  }
};


module.exports = new Console(process.stdout, process.stderr, process._logger);
module.exports.Console = Console;
