'use strict';

const {
  ArrayIsArray,
  Map,
  MapPrototypeSet,
  ObjectCreate,
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
let debug = require('internal/util/debuglog').debuglog('policy', (fn) => {
  debug = fn;
});
const SRI = require('internal/policy/sri');
const crypto = require('crypto');
const { Buffer } = require('buffer');
const { URL } = require('internal/url');
const { createHash, timingSafeEqual } = crypto;
const HashUpdate = uncurryThis(crypto.Hash.prototype.update);
const HashDigest = uncurryThis(crypto.Hash.prototype.digest);
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
  /**
   * @type {Map<string, true | string | SRI[]>}
   *
   * Used to compare a resource to the content body at the resource.
   * `true` is used to signify that all integrities are allowed, otherwise,
   * SRI strings are parsed to compare with the body.
   *
   * This stores strings instead of eagerly parsing SRI strings
   * and only converts them to SRI data structures when needed.
   * This avoids needing to parse all SRI strings at startup even
   * if some never end up being used.
   */
  #integrities = new SafeMap();
  /**
   * @type {Map<string, (specifier: string) => true | URL>}
   *
   * Used to find where a dependency is located.
   *
   * This stores functions to lazily calculate locations as needed.
   * `true` is used to signify that the location is not specified
   * by the manifest and default resolution should be allowed.
   */
  #dependencies = new SafeMap();
  /**
   * @type {(err: Error) => void}
   *
   * Performs default action for what happens when a manifest encounters
   * a violation such as abort()ing or exiting the process, throwing the error,
   * or logging the error.
   */
  #reaction = null;
  /**
   * `obj` should match the policy file format described in the docs
   * it is expected to not have prototype pollution issues either by reassigning
   * the prototype to `null` for values or by running prior to any user code.
   *
   * `manifestURL` is a URL to resolve relative locations against.
   *
   * @param {object} obj
   * @param {string} manifestURL
   */
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
        debug('Manifest contains integrity for url %s', originalHREF);
        if (typeof integrity === 'string') {
          if (integrities.has(resourceHREF)) {
            if (integrities.get(resourceHREF) !== integrity) {
              throw new ERR_MANIFEST_INTEGRITY_MISMATCH(resourceURL);
            }
          }
          integrities.set(resourceHREF, integrity);
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
        dependencyMap = ObjectCreate(null);
      }
      if (typeof dependencyMap === 'object' && !ArrayIsArray(dependencyMap)) {
        /**
         * @returns {true | URL}
         */
        const dependencyRedirectList = (toSpecifier) => {
          if (toSpecifier in dependencyMap !== true) {
            return null;
          }
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
    debug('Checking integrity of %s', href);
    const integrities = this.#integrities;
    const realIntegrities = new Map();

    if (integrities.has(href)) {
      let integrityEntries = integrities.get(href);
      if (integrityEntries === true) return true;
      if (typeof integrityEntries === 'string') {
        const sri = ObjectFreeze(SRI.parse(integrityEntries));
        integrities.set(href, sri);
        integrityEntries = sri;
      }
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
