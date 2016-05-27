
/**
 * Module dependencies.
 */

var Emitter = require('events').EventEmitter;
var bindings = require('bindings')('weakref.node');

/**
 * Set global weak callback function.
 */

bindings._setCallback(callback);

/**
 * Module exports.
 */

exports = module.exports = create;
exports.addCallback = exports.addListener = addCallback;
exports.removeCallback = exports.removeListener = removeCallback;
exports.removeCallbacks = exports.removeListeners = removeCallbacks;
exports.callbacks = exports.listeners = callbacks;

// backwards-compat with node-weakref
exports.weaken = exports.create = exports;

// re-export all the binding functions onto the exports
Object.keys(bindings).forEach(function (name) {
  exports[name] = bindings[name];
});

/**
 * Internal emitter event name.
 * This is completely arbitrary...
 * Could be any value....
 */

var CB = '_CB';

/**
 * Creates and returns a new Weakref instance. Optionally attaches
 * a weak callback to invoke when the Object gets garbage collected.
 *
 * @api public
 */

function create (obj, fn) {
  var weakref = bindings._create(obj, new Emitter());
  if ('function' == typeof fn) {
    exports.addCallback(weakref, fn);
  }
  return weakref;
}

/**
 * Adds a weak callback function to the Weakref instance.
 *
 * @api public
 */

function addCallback (weakref, fn) {
  var emitter = bindings._getEmitter(weakref);
  return emitter.on(CB, fn);
}

/**
 * Removes a weak callback function from the Weakref instance.
 *
 * @api public
 */

function removeCallback (weakref, fn) {
  var emitter = bindings._getEmitter(weakref);
  return emitter.removeListener(CB, fn);
}

/**
 * Returns a copy of the listeners on the Weakref instance.
 *
 * @api public
 */

function callbacks (weakref) {
  var emitter = bindings._getEmitter(weakref);
  return emitter.listeners(CB);
}


/**
 * Removes all callbacks on the Weakref instance.
 *
 * @api public
 */

function removeCallbacks (weakref) {
  var emitter = bindings._getEmitter(weakref);
  return emitter.removeAllListeners(CB);
}

/**
 * Common weak callback function.
 *
 * @api private
 */

function callback (emitter) {
  emitter.emit(CB);
  emitter = null;
}
