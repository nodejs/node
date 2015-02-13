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

var util = require('util');
var assert = require('assert');

function Console(stdout, stderr) {
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
  prop.value = [];
  Object.defineProperty(this, '_times', prop);

  // bind the prototype functions to this Console instance
  Object.keys(Console.prototype).forEach(function(k) {
    this[k] = this[k].bind(this);
  }, this);
}

Console.prototype.log = function() {
  this._stdout.write(util.format.apply(this, arguments) + '\n');
};


Console.prototype.info = Console.prototype.log;


Console.prototype.warn = function() {
  this._stderr.write(util.format.apply(this, arguments) + '\n');
};


Console.prototype.error = Console.prototype.warn;


Console.prototype.dir = function(object) {
  this._stdout.write(util.inspect(object) + '\n');
};


function findTimeWithLabel(times, label) {
  assert(Array.isArray(times), 'times must be an Array');
  assert(typeof label === 'string', 'label must be a string');

  var found;

  times.some(function findTime(item) {
    if (item.label === label) {
      found = item;
      return true;
    }
  });

  return found;
}

Console.prototype.time = function(label) {
  var timeEntry = findTimeWithLabel(this._times, label);
  var wallClockTime = Date.now();
  if (!timeEntry) {
    this._times.push({ label: label, time: wallClockTime });
  } else {
    timeEntry.time = wallClockTime;
  }
};

Console.prototype.timeEnd = function(label) {
  var wallClockTimeEnd = Date.now();
  var timeEntry = findTimeWithLabel(this._times, label);

  if (!timeEntry) {
    throw new Error('No such label: ' + label);
  } else {
    assert(typeof timeEntry.time === 'number',
           'start time must be a number');
    var duration = wallClockTimeEnd - timeEntry.time;
    this.log('%s: %dms', label, duration);
  }
};


Console.prototype.trace = function() {
  // TODO probably can to do this better with V8's debug object once that is
  // exposed.
  var err = new Error;
  err.name = 'Trace';
  err.message = util.format.apply(this, arguments);
  Error.captureStackTrace(err, arguments.callee);
  this.error(err.stack);
};


Console.prototype.assert = function(expression) {
  if (!expression) {
    var arr = Array.prototype.slice.call(arguments, 1);
    require('assert').ok(false, util.format.apply(this, arr));
  }
};


module.exports = new Console(process.stdout, process.stderr);
module.exports.Console = Console;
