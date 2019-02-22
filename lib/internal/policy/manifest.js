'use strict';
const {
  ERR_MANIFEST_ASSERT_INTEGRITY,
  ERR_MANIFEST_INTEGRITY_MISMATCH,
  ERR_MANIFEST_UNKNOWN_ONERROR,
} = require('internal/errors').codes;
const debug = require('util').debuglog('policy');
const SRI = require('internal/policy/sri');
const {
  SafeWeakMap,
  FunctionPrototype,
  Object,
  RegExpPrototype
} = primordials;
const crypto = require('crypto');
const { Buffer } = require('buffer');
const { URL } = require('url');
const { createHash, timingSafeEqual } = crypto;
const HashUpdate = FunctionPrototype.call.bind(crypto.Hash.prototype.update);
const HashDigest = FunctionPrototype.call.bind(crypto.Hash.prototype.digest);
const BufferEquals = FunctionPrototype.call.bind(Buffer.prototype.equals);
const BufferToString = FunctionPrototype.call.bind(Buffer.prototype.toString);
const RegExpTest = FunctionPrototype.call.bind(RegExpPrototype.test);
const { entries } = Object;
const kIntegrities = new SafeWeakMap();
const kReactions = new SafeWeakMap();
const kRelativeURLStringPattern = /^\.{0,2}\//;
const { getOptionValue } = require('internal/options');
const shouldAbortOnUncaughtException =
  getOptionValue('--abort-on-uncaught-exception');
const { abort, exit, _rawDebug } = process;

function REACTION_THROW(error) {
  throw error;
}

function REACTION_EXIT(error) {
  REACTION_LOG(error);
  if (shouldAbortOnUncaughtException) {
    abort();
  }
  exit(1);
}

function REACTION_LOG(error) {
  _rawDebug(error.stack);
}

class Manifest {
  constructor(obj, manifestURL) {
    const integrities = {
      __proto__: null,
    };
    const reactions = {
      __proto__: null,
      integrity: REACTION_THROW,
    };

    if (obj.onerror) {
      const behavior = obj.onerror;
      if (behavior === 'throw') {
      } else if (behavior === 'exit') {
        reactions.integrity = REACTION_EXIT;
      } else if (behavior === 'log') {
        reactions.integrity = REACTION_LOG;
      } else {
        throw new ERR_MANIFEST_UNKNOWN_ONERROR(behavior);
      }
    }

    kReactions.set(this, Object.freeze(reactions));
    const manifestEntries = entries(obj.resources);

    for (var i = 0; i < manifestEntries.length; i++) {
      let url = manifestEntries[i][0];
      const integrity = manifestEntries[i][1].integrity;
      if (integrity != null) {
        debug(`Manifest contains integrity for url ${url}`);
        if (RegExpTest(kRelativeURLStringPattern, url)) {
          url = new URL(url, manifestURL).href;
        }

        const sri = Object.freeze(SRI.parse(integrity));
        if (url in integrities) {
          const old = integrities[url];
          let mismatch = false;

          if (old.length !== sri.length) {
            mismatch = true;
          } else {
            compare:
            for (var sriI = 0; sriI < sri.length; sriI++) {
              for (var oldI = 0; oldI < old.length; oldI++) {
                if (sri[sriI].algorithm === old[oldI].algorithm &&
                  BufferEquals(sri[sriI].value, old[oldI].value) &&
                  sri[sriI].options === old[oldI].options) {
                  continue compare;
                }
              }
              mismatch = true;
              break compare;
            }
          }

          if (mismatch) {
            throw new ERR_MANIFEST_INTEGRITY_MISMATCH(url);
          }
        }
        integrities[url] = sri;
      }
    }
    Object.freeze(integrities);
    kIntegrities.set(this, integrities);
    Object.freeze(this);
  }

  assertIntegrity(url, content) {
    debug(`Checking integrity of ${url}`);
    const integrities = kIntegrities.get(this);
    const realIntegrities = new Map();

    if (integrities && url in integrities) {
      const integrityEntries = integrities[url];
      // Avoid clobbered Symbol.iterator
      for (var i = 0; i < integrityEntries.length; i++) {
        const {
          algorithm,
          value: expected
        } = integrityEntries[i];
        const hash = createHash(algorithm);
        HashUpdate(hash, content);
        const digest = HashDigest(hash);
        if (digest.length === expected.length &&
          timingSafeEqual(digest, expected)) {
          return true;
        }
        realIntegrities.set(algorithm, BufferToString(digest, 'base64'));
      }
    }
    const error = new ERR_MANIFEST_ASSERT_INTEGRITY(url, realIntegrities);
    kReactions.get(this).integrity(error);
  }
}

// Lock everything down to avoid problems even if reference is leaked somehow
Object.setPrototypeOf(Manifest, null);
Object.setPrototypeOf(Manifest.prototype, null);
Object.freeze(Manifest);
Object.freeze(Manifest.prototype);
module.exports = Object.freeze({ Manifest });
