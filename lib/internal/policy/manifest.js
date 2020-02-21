'use strict';

const {
  ArrayIsArray,
  Map,
  MapPrototypeSet,
  ObjectEntries,
  ObjectFreeze,
  ObjectSetPrototypeOf,
  RegExpPrototypeTest,
  SafeMap,
  uncurryThis,
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
const { URL } = require('internal/url');
const { createHash, timingSafeEqual } = crypto;
const HashUpdate = uncurryThis(crypto.Hash.prototype.update);
const HashDigest = uncurryThis(crypto.Hash.prototype.digest);
const BufferEquals = uncurryThis(Buffer.prototype.equals);
const BufferToString = uncurryThis(Buffer.prototype.toString);
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
  #integrities = new SafeMap();
  #dependencies = new SafeMap();
  #reaction = null;
  constructor(obj, manifestURL) {
    const integrities = this.#integrities;
    const dependencies = this.#dependencies;
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

    this.#reaction = reaction;
    const manifestEntries = ObjectEntries(obj.resources);

    const parsedURLs = new SafeMap();
    for (let i = 0; i < manifestEntries.length; i++) {
      let resourceHREF = manifestEntries[i][0];
      const originalHREF = resourceHREF;
      let resourceURL;
      if (parsedURLs.has(resourceHREF)) {
        resourceURL = parsedURLs.get(resourceHREF);
        resourceHREF = resourceURL.href;
      } else if (
        RegExpPrototypeTest(kRelativeURLStringPattern, resourceHREF)
      ) {
        resourceURL = new URL(resourceHREF, manifestURL);
        resourceHREF = resourceURL.href;
        parsedURLs.set(originalHREF, resourceURL);
        parsedURLs.set(resourceHREF, resourceURL);
      }
      let integrity = manifestEntries[i][1].integrity;
      if (!integrity) integrity = null;
      if (integrity != null) {
        debug(`Manifest contains integrity for url ${originalHREF}`);
        if (typeof integrity === 'string') {
          const sri = ObjectFreeze(SRI.parse(integrity));
          if (integrities.has(resourceHREF)) {
            const old = integrities.get(resourceHREF);
            let mismatch = false;

            if (old.length !== sri.length) {
              mismatch = true;
            } else {
              compare:
              for (let sriI = 0; sriI < sri.length; sriI++) {
                for (let oldI = 0; oldI < old.length; oldI++) {
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
              throw new ERR_MANIFEST_INTEGRITY_MISMATCH(resourceURL);
            }
          }
          integrities.set(resourceHREF, sri);
        } else if (integrity === true) {
          integrities.set(resourceHREF, true);
        } else {
          throw new ERR_MANIFEST_INVALID_RESOURCE_FIELD(
            resourceHREF,
            'integrity');
        }
      }

      let dependencyMap = manifestEntries[i][1].dependencies;
      if (dependencyMap === null || dependencyMap === undefined) {
        dependencyMap = {};
      }
      if (typeof dependencyMap === 'object' && !ArrayIsArray(dependencyMap)) {
        /**
         * @returns {true | URL}
         */
        const dependencyRedirectList = (toSpecifier) => {
          if (toSpecifier in dependencyMap !== true) {
            return null;
          } else {
            const to = dependencyMap[toSpecifier];
            if (to === true) {
              return true;
            }
            if (parsedURLs.has(to)) {
              return parsedURLs.get(to);
            } else if (canBeRequiredByUsers(to)) {
              const href = `node:${to}`;
              const resolvedURL = new URL(href);
              parsedURLs.set(to, resolvedURL);
              parsedURLs.set(href, resolvedURL);
              return resolvedURL;
            } else if (RegExpPrototypeTest(kRelativeURLStringPattern, to)) {
              const resolvedURL = new URL(to, manifestURL);
              const href = resourceURL.href;
              parsedURLs.set(to, resolvedURL);
              parsedURLs.set(href, resolvedURL);
              return resolvedURL;
            }
            const resolvedURL = new URL(to);
            const href = resourceURL.href;
            parsedURLs.set(to, resolvedURL);
            parsedURLs.set(href, resolvedURL);
            return resolvedURL;
          }
        };
        dependencies.set(resourceHREF, dependencyRedirectList);
      } else if (dependencyMap === true) {
        const arbitraryDependencies = () => true;
        dependencies.set(resourceHREF, arbitraryDependencies);
      } else {
        throw new ERR_MANIFEST_INVALID_RESOURCE_FIELD(
          resourceHREF,
          'dependencies');
      }
    }
    ObjectFreeze(this);
  }

  getRedirector(requester) {
    requester = `${requester}`;
    const dependencies = this.#dependencies;
    if (dependencies.has(requester)) {
      return {
        resolve: (to) => dependencies.get(requester)(`${to}`),
        reaction: this.#reaction
      };
    }
    return null;
  }

  assertIntegrity(url, content) {
    const href = `${url}`;
    debug(`Checking integrity of ${href}`);
    const integrities = this.#integrities;
    const realIntegrities = new Map();

    if (integrities.has(href)) {
      const integrityEntries = integrities.get(href);
      if (integrityEntries === true) return true;
      // Avoid clobbered Symbol.iterator
      for (let i = 0; i < integrityEntries.length; i++) {
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
        MapPrototypeSet(
          realIntegrities,
          algorithm,
          BufferToString(digest, 'base64')
        );
      }
    }
    const error = new ERR_MANIFEST_ASSERT_INTEGRITY(url, realIntegrities);
    this.#reaction(error);
  }
}

// Lock everything down to avoid problems even if reference is leaked somehow
ObjectSetPrototypeOf(Manifest, null);
ObjectSetPrototypeOf(Manifest.prototype, null);
ObjectFreeze(Manifest);
ObjectFreeze(Manifest.prototype);
module.exports = ObjectFreeze({ Manifest });
