'use strict';

const {
  SafeMap,
  SafeWeakMap,
  Object,
  RegExpPrototype,
  uncurryThis
} = primordials;
const {
  canBeRequiredByUsers
} = require('internal/bootstrap/loaders').NativeModule;

const {
  ERR_MANIFEST_ASSERT_INTEGRITY,
  ERR_MANIFEST_INTEGRITY_MISMATCH,
  ERR_MANIFEST_INVALID_RESOURCE_FIELD,
  ERR_MANIFEST_UNKNOWN_ONERROR,
} = require('internal/errors').codes;
const debug = require('internal/util/debuglog').debuglog('policy');
const SRI = require('internal/policy/sri');
const crypto = require('crypto');
const { Buffer } = require('buffer');
const { URL } = require('url');
const { createHash, timingSafeEqual } = crypto;
const HashUpdate = uncurryThis(crypto.Hash.prototype.update);
const HashDigest = uncurryThis(crypto.Hash.prototype.digest);
const BufferEquals = uncurryThis(Buffer.prototype.equals);
const BufferToString = uncurryThis(Buffer.prototype.toString);
const { entries } = Object;
const kIntegrities = new SafeWeakMap();
const kDependencies = new SafeWeakMap();
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
    const dependencies = {
      __proto__: null,
    };
    let reaction = REACTION_THROW;

    if (obj.onerror) {
      const behavior = obj.onerror;
      if (behavior === 'throw') {
      } else if (behavior === 'exit') {
        reaction = REACTION_EXIT;
      } else if (behavior === 'log') {
        reaction = REACTION_LOG;
      } else {
        throw new ERR_MANIFEST_UNKNOWN_ONERROR(behavior);
      }
    }

    kReactions.set(this, reaction);
    const manifestEntries = entries(obj.resources);

    for (var i = 0; i < manifestEntries.length; i++) {
      let url = manifestEntries[i][0];
      const originalURL = url;
      if (RegExpPrototype.test(kRelativeURLStringPattern, url)) {
        url = new URL(url, manifestURL).href;
      }
      let integrity = manifestEntries[i][1].integrity;
      if (!integrity) integrity = null;
      if (integrity != null) {
        debug(`Manifest contains integrity for url ${originalURL}`);
        if (typeof integrity === 'string') {
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
        } else if (integrity === true) {
          integrities[url] = true;
        } else {
          throw new ERR_MANIFEST_INVALID_RESOURCE_FIELD(url, 'integrity');
        }
      }

      let dependencyMap = manifestEntries[i][1].dependencies;
      if (dependencyMap === null || dependencyMap === undefined) {
        dependencyMap = {};
      }
      if (typeof dependencyMap === 'object' && !Array.isArray(dependencyMap)) {
        dependencies[url] = new SafeMap(Object.entries(dependencyMap).map(
          ([ from, to ]) => {
            if (to === true) {
              return [from, to];
            }
            if (canBeRequiredByUsers(to)) {
              return [from, `node:${to}`];
            } else if (RegExpPrototype.test(kRelativeURLStringPattern, to)) {
              return [from, new URL(to, manifestURL).href];
            }
            return [from, new URL(to).href];
          })
        );
      } else if (dependencyMap === true) {
        dependencies[url] = true;
      } else {
        throw new ERR_MANIFEST_INVALID_RESOURCE_FIELD(url, 'dependencies');
      }
    }
    Object.freeze(integrities);
    kIntegrities.set(this, integrities);
    Object.freeze(dependencies);
    kDependencies.set(this, dependencies);
    Object.freeze(this);
  }

  getRedirects(requester) {
    const dependencies = kDependencies.get(this);
    if (dependencies && requester in dependencies) {
      return {
        map: dependencies[requester],
        reaction: kReactions.get(this)
      };
    }
    return null;
  }

  assertIntegrity(url, content) {
    debug(`Checking integrity of ${url}`);
    const integrities = kIntegrities.get(this);
    const realIntegrities = new SafeMap();

    if (integrities && url in integrities) {
      const integrityEntries = integrities[url];
      if (integrityEntries === true) return true;
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
    kReactions.get(this)(error);
  }
}

// Lock everything down to avoid problems even if reference is leaked somehow
Object.setPrototypeOf(Manifest, null);
Object.setPrototypeOf(Manifest.prototype, null);
Object.freeze(Manifest);
Object.freeze(Manifest.prototype);
module.exports = Object.freeze({ Manifest });
