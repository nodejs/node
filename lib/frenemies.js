'use strict';

/**
 * Allow modules to establish secure channels to other modules.
 * This is a general purpose building block that should eventually enable:
 * - product teams to grant more privilege to dependencies they
 *   know are compatible with their security needs than to the
 *   bulk of dependencies.
 * - allow conveying sensitive data (secrets, passwords, PII) from
 *   one part of a system to another that are opaque to cross-cutting
 *   concerns like logging.
 * - allow modules that provide APIs that are unsafe when not used
 *   very carefully to interact closely with carefully vetted modules
 *   but failsafe when used by general purpose code.
 */

// Capture some globals so we can rely on them later
const { Error, WeakMap, undefined } = global;
const { create, defineProperties, freeze } = Object;
const { SafeWeakMap, SafeWeakSet } = require('internal/safe_globals');

/**
 * Maps opaque boxes to box data records.
 */
const boxes = new SafeWeakMap();

/**
 * A set of all public keys.
 */
const publicKeys = new SafeWeakSet();

/**
 * True iff the given function is in fact a public key.
 *
 * Public keys are represented as functions that return
 * the first of two arguments if called during the execution
 * of their matching private key, or otherwise return their
 * second argument.
 *
 * Their arguments default to (true, false).
 */
const isPublicKey = publicKeys.has.bind(publicKeys);

/** An opaque token used to represent a boxed value in transit. */
class Box {
  toString() { return '[Box]'; }
}

/**
 * Space for collaboration between the private and public
 * halves of a public/private key pair.
 */
let hidden = undefined;

/**
 * Creates a bundle that should be available as
 * a local variable to module code.
 */
function makeFrenemies(moduleIdentifier) {
  // Allocate a public/private key pair.
  function publicKey(a = true, b = false) {
    return (hidden === privateKey) ? a : b;
  }
  function privateKey(f) {
    const previous = hidden;

    hidden = privateKey;
    try {
      return f();
    } finally {
      hidden = previous;
    }
  }
  publicKeys.add(publicKey);

  // We attach a module identifier to the public key to ena to enable
  // whitelisting based on strings in a configuration without loading
  // modules to allocate their public key to store in a set.
  // This may be less robust than private/public key pair checking.
  defineProperties(
    publicKey,
    {
      moduleIdentifier: { value: '' + moduleIdentifier, enumerable: true },
      call: { value: Function.prototype.call, enumerable: true },
      apply: { value: Function.prototype.apply, enumerable: true }
    });

  /**
   * Wraps a value in a box so that only an approved
   * opener may unbox it.
   *
   * @param value the value that will be given to
   *    an approved unboxer.
   * @param mayOpen receives the public key of the opener.
   *    Should return `true` to allow.
   *    This will be called in the context of the opener's
   *    private key, so the public key should also return true
   *    called with no arguments.
   * @return a box that is opaque to any receivers that cannot
   *    unbox it.
   */
  function box(value, mayOpen) {
    if (typeof mayOpen !== 'function') {
      throw new Error(`Expected function not ${mayOpen}`);
    }
    const box = new Box();  // An opaque token
    boxes.set(
      box,
      freeze({ boxerPriv: privateKey, boxerPub: publicKey, value, mayOpen }));
    return box;
  }

  // TODO should this be async so it can await the box?
  /**
   * Tries to open a box.
   *
   * @param box the box to unbox.
   * @param ifFrom if the box may be opened by this unboxer's owner,
   *    then ifFrom receives the publicKey of the box creator.
   *    It should return true to allow unboxing to proceed.
   *    TODO: Is this the object identity equivalent of an
   *    out-of-order verify/decrypt fault?
   *    http://world.std.com/~dtd/sign_encrypt/sign_encrypt7.html
   * @param fallback a value to substitute if unboxing failed.
   *    Defaults to undefined.
   * @return the value if unboxing is allowed or fallback otherwise.
   */
  function unbox(box, ifFrom, fallback) {
    if (typeof ifFrom !== 'function') {
      throw new Error(`Expected function not ${ifFrom}`);
    }
    const data = boxes.get(box);
    if (!data) { return fallback; }
    const { boxerPriv, boxerPub, value, mayOpen } = data;
    // Require mutual consent
    return (true === privateKey(() => mayOpen(publicKey)) &&
            true === boxerPriv(() => ifFrom(boxerPub))) ?
      value :
      fallback;
  }

  const neverBoxed = {};
  /** Like unbox but raises an exception if unboxing fails. */
  function unboxStrict(box, ifFrom) {
    const result = unbox(box, ifFrom, neverBoxed);
    if (result === neverBoxed) {
      throw new Error('Could not unbox');
    }
    return result;
  }

  return defineProperties(
    create(null),
    {
      // These close over private keys, so do not leak them.
      box: { value: box, enumerable: true },
      unbox: { value: unbox, enumerable: true },
      unboxStrict: { value: unboxStrict, enumerable: true },
      privateKey: { value: privateKey, enumerable: true },
      isPublicKey: { value: isPublicKey, enumerable: true },

      // Modules may allow access to this, perhaps via
      // module object.
      publicKey: { value: publicKey, enumerable: true }
    });
}

module.exports = freeze({ makeFrenemies });
