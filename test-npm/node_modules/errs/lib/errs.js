/*
 * errs.js: Simple error creation and passing utilities.
 *
 * (C) 2012, Charlie Robbins, Nuno Job, and the Contributors.
 * MIT LICENSE
 *
 */

var events = require('events'),
    util = require('util');

//
// Container for registered error types.
//
exports.registered = {};

//
// Add `Error.prototype.toJSON` if it doesn't exist.
//
if (!Error.prototype.toJSON) {
  Object.defineProperty(Error.prototype, 'toJSON', {
    enumerable: false,
    writable: true,
    value: function () {
      return mixin({
        message: this.message,
        stack: this.stack,
        arguments: this.arguments,
        type: this.type
      }, this);
    }
  });
}

//
// ### function create (type, opts)
// #### @type {string} **Optional** Registered error type to create
// #### @opts {string|object|Array|function} Options for creating the error:
//   * `string`:   Message for the error
//   * `object`:   Properties to include on the error
//   * `array`:    Message for the error (' ' joined).
//   * `function`: Function to return error options.
//
// Creates a new error instance for with the specified `type`
// and `options`. If the `type` is not registered then a new
// `Error` instance will be created.
//
exports.create = function createErr(type, opts) {
  if (!arguments[1] && !exports.registered[type]) {
    opts = type;
    type = null;
  }

  //
  // If the `opts` has a `stack` property assume
  // that it is already an error instance.
  //
  if (opts && opts.stack) {
    return opts;
  }

  var message,
      ErrorProto,
      error;

  //
  // Parse arguments liberally for the message
  //
  if (typeof opts === 'function') {
    opts = opts();
  }

  if (Array.isArray(opts)) {
    message = opts.join(' ');
    opts = null;
  }
  else if (opts) {
    switch (typeof opts) {
      case 'string':
        message = opts || 'Unspecified error';
        opts = null;
        break;
      case 'object':
        message = (opts && opts.message) || 'Unspecified error';
        break;
      default:
        message = 'Unspecified error';
        break;
    }
  }

  //
  // Instantiate a new Error instance or a new
  // registered error type (if it exists).
  //
  ErrorProto = type && exports.registered[type] || Error;
  error = new (ErrorProto)(message);

  if (!error.name || error.name === 'Error') {
    error.name = (opts && opts.name) || ErrorProto.name || 'Error';
  }

  //
  // Capture a stack trace if it does not already exist and
  // remote the part of the stack trace referencing `errs.js`.
  //
  if (!error.stack) {
    Error.call(error);
    Error.captureStackTrace(error, createErr);
  }
  else {
    error.stack = error.stack.split('\n');
    error.stack.splice(1, 1);
    error.stack = error.stack.join('\n');
  }

  //
  // Copy all options to the new error instance.
  //
  if (opts) {
    Object.keys(opts).forEach(function (key) {
      error[key] = opts[key];
    });
  }

  return error;
};

//
// ### function merge (err, type, opts)
// #### @err  {error}  **Optional** The error to merge
// #### @type {string} **Optional** Registered error type to create
// #### @opts {string|object|Array|function} Options for creating the error:
//   * `string`:   Message for the error
//   * `object`:   Properties to include on the error
//   * `array`:    Message for the error (' ' joined).
//   * `function`: Function to return error options.
//
// Merges an existing error with a new error instance for with
// the specified `type` and `options`.
//
exports.merge = function (err, type, opts) {
  var merged = exports.create(type, opts);

  //
  // If there is no error just return the merged one
  //
  if (err == undefined || err == null) {
    return merged;
  }

  //
  // optional stuff that might be created by module
  //
  if (!Array.isArray(err) && typeof err === 'object') {
    Object.keys(err).forEach(function (key) {
      //
      // in node v0.4 v8 errors where treated differently
      // we need to make sure we aren't merging these properties
      // http://code.google.com/p/v8/issues/detail?id=1215
      //
      if (['stack', 'type', 'arguments', 'message'].indexOf(key)===-1) {
        merged[key] = err[key];
      }
    });
  }

  // merging
  merged.name = merged.name || err.name;
  merged.message = merged.message || err.message;

  // override stack
  merged.stack = err.stack || merged.stack;

  // add human-readable errors
  if (err.message) {
    merged.description = err.message;
  }

  if (err.stack && err.stack.split) {
    merged.stacktrace = err.stack.split("\n");
  }

  return merged;
};

//
// ### function handle (error, callback)
// #### @error {string|function|Array|object} Error to handle
// #### @callback {function|EventEmitter} **Optional** Continuation or stream to pass the error to.
// #### @stream {EventEmitter} **Optional** Explicit EventEmitter to use.
//
// Attempts to instantiate the given `error`. If the `error` is already a properly
// formed `error` object (with a `stack` property) it will not be modified.
//
// * If `callback` is a function, it is invoked with the `error`.
// * If `callback` is an `EventEmitter`, it emits the `error` event on
//   that emitter and returns it.
// * If no `callback`, return a new `EventEmitter` which emits `error`
//   on `process.nextTick()`.
//
exports.handle = function (error, callback, stream) {
  error = exports.create(error);

  if (typeof callback === 'function') {
    callback(error);
  }

  if (typeof callback !== 'function' || stream) {
    var emitter = stream || callback || new events.EventEmitter();
    process.nextTick(function () { emitter.emit('error', error); });
    return emitter;
  }
};

//
// ### function register (type, proto)
// #### @type {string} **Optional** Type of the error to register.
// #### @proto {function} Constructor function of the error to register.
//
// Registers the specified `proto` to `type` for future calls to
// `errors.create(type, opts)`.
//
exports.register = function (type, proto) {
  if (arguments.length === 1) {
    proto = type;
    type = proto.name.toLowerCase();
  }
  exports.registered[type] = proto;
};

//
// ### function unregister (type)
// #### @type {string} Type of the error to unregister.
//
// Unregisters the specified `type` for future calls to
// `errors.create(type, opts)`.
//
exports.unregister = function (type) {
  delete exports.registered[type];
};

//
// ### function mixin (target [source0, source1, ...])
// Copies enumerable properties from `source0 ... sourceN`
// onto `target` and returns the resulting object.
//
function mixin(target) {
  //
  // Quickly and performantly (in V8) `Arrayify` arguments.
  //
  var len = arguments.length,
      args = new Array(len - 1),
      i = 1;

  for (; i < len; i++) {
    args[i - 1] = arguments[i];
  }

  args.forEach(function (o) {
    Object.keys(o).forEach(function (attr) {
      var getter = o.__lookupGetter__(attr),
          setter = o.__lookupSetter__(attr);

      if (!getter && !setter) {
        target[attr] = o[attr];
      }
      else {
        if (setter) { target.__defineSetter__(attr, setter) }
        if (getter) { target.__defineGetter__(attr, getter) }
      }
    });
  });

  return target;
}
