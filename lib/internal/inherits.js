'use strict';

/**
 * Inherit the prototype methods from one constructor into another.
 *
 * The Function.prototype.inherits from lang.js rewritten as a standalone
 * function (not on Function.prototype). NOTE: If this file is to be loaded
 * during bootstrapping this function needs to be rewritten using some native
 * functions as prototype setup using normal JavaScript does not work as
 * expected during bootstrapping (see mirror.js in r114903).
 *
 * @param {function} ctor Constructor function which needs to inherit the
 *     prototype.
 * @param {function} superCtor Constructor function to inherit prototype from.
 * @throws {TypeError} Will error if either constructor is null, or if
 *     the super constructor lacks a prototype.
 */
module.exports = function inherits(ctor, superCtor) {
  if (ctor === undefined || ctor === null)
    throw new TypeError('The constructor to `inherits` must not be ' +
                        'null or undefined.');

  if (superCtor === undefined || superCtor === null)
    throw new TypeError('The super constructor to `inherits` must not ' +
                        'be null or undefined.');

  if (superCtor.prototype === undefined)
    throw new TypeError('The super constructor to `inherits` must ' +
                        'have a prototype.');

  ctor.super_ = superCtor;
  ctor.prototype = Object.create(superCtor.prototype, {
    constructor: {
      value: ctor,
      enumerable: false,
      writable: true,
      configurable: true
    }
  });
};
