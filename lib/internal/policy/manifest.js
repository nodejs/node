'use strict';

const {
  ArrayIsArray,
  ObjectCreate,
  ObjectEntries,
  ObjectFreeze,
  ObjectKeys,
  ObjectSetPrototypeOf,
  RegExpPrototypeTest,
  SafeMap,
  SafeSet,
  StringPrototypeEndsWith,
  StringPrototypeReplace,
  Symbol,
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

const kTerminate = () => null;

// From https://url.spec.whatwg.org/#special-scheme
const kSpecialSchemes = new SafeSet([
  'file:',
  'ftp:',
  'http:',
  'https:',
  'ws:',
  'wss:',
]);

const kCascade = Symbol('cascade');

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
/**
 * @typedef {(specifier: string) => true | URL} DependencyMapper
 * @typedef {Record<string, string> | true} DependencyMap
 * @typedef {true | string | SRI[]} Integrity
 */

class Manifest {
  /**
   * @type {Map<string | null | undefined, DependencyMapper>}
   *
   * Used to compare a resource to the content body at the resource.
   * `true` is used to signify that all integrities are allowed, otherwise,
   * SRI strings are parsed to compare with the body.
   *
   * Separate from #resourceDependencies due to conflicts with things like
   * `blob:` being both a scope and a resource potentially as well as
   * `file:` being parsed to `file:///` instead of remaining host neutral.
   */
  #scopeDependencies = new SafeMap();
  /**
   * @type {Map<string, true | null | 'cascade'>}
   *
   * Used to allow arbitrary loading within a scope
   */
  #scopeIntegrities = new SafeMap();
  /**
   * @type {Map<string, Integrity>}
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
  #resourceIntegrities = new SafeMap();
  /**
   * @type {Map<string, DependencyMapper>}
   *
   * Used to find where a dependency is located.
   *
   * This stores functions to lazily calculate locations as needed.
   * `true` is used to signify that the location is not specified
   * by the manifest and default resolution should be allowed.
   *
   * The functions return `null` to signify that a dependency is
   * not found
   */
  #resourceDependencies = new SafeMap();
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
    const scopes = this.#scopeDependencies;
    scopes.set(null, kTerminate);
    scopes.set(undefined, kTerminate);
    const integrities = this.#resourceIntegrities;
    const dependencies = this.#resourceDependencies;
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
    const jsonResourcesEntries = ObjectEntries(obj.resources ?? {});
    const jsonScopesEntries = ObjectEntries(obj.scopes ?? {});

    function searchDependencies(resourceHREF, target, conditions) {
      if (
        target &&
        typeof target === 'object' &&
        !ArrayIsArray(target)
      ) {
        const keys = ObjectKeys(target);
        for (let i = 0; i < keys.length; i++) {
          const key = keys[i];
          if (conditions.has(key)) {
            const ret = searchDependencies(
              resourceHREF,
              target[key],
              conditions);
            if (ret != null) {
              return ret;
            }
          }
        }
      } else if (typeof target === 'string') {
        return target;
      } else if (target === true) {
        return target;
      } else {
        throw new ERR_MANIFEST_INVALID_RESOURCE_FIELD(
          resourceHREF,
          'dependencies');
      }
      return null;
    }

    /**
     * @param {string} resourceHREF
     * @param {{[key: string]: string | true}} dependencyMap
     * @param {boolean} cascade
     * @returns {DependencyMapper}
     */
    const createDependencyMapper = (
      resourceHREF,
      dependencyMap,
      cascade
    ) => {
      let parentDeps;
      return (toSpecifier, conditions) => {
        if (toSpecifier in dependencyMap !== true) {
          if (cascade === true) {
            /** @type {string | null} */
            let scopeHREF = resourceHREF;
            if (typeof parentDeps === 'undefined') {
              do {
                scopeHREF = this.#findScopeHREF(scopeHREF);
                if (scopeHREF === resourceHREF) {
                  scopeHREF = null;
                }
                if (scopes.has(scopeHREF)) {
                  break;
                }
              } while (
                scopeHREF !== null
              );
              parentDeps = scopes.get(scopeHREF);
            }
            return parentDeps(toSpecifier);
          }
          return null;
        }
        const to = searchDependencies(
          resourceHREF,
          dependencyMap[toSpecifier],
          conditions);
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
          const href = resourceHREF;
          parsedURLs.set(to, resolvedURL);
          parsedURLs.set(href, resolvedURL);
          return resolvedURL;
        }
        const resolvedURL = new URL(to);
        const href = resourceHREF;
        parsedURLs.set(to, resolvedURL);
        parsedURLs.set(href, resolvedURL);
        return resolvedURL;
      };
    };

    /**
     * Stores URLs keyed by string specifier relative to the manifest
     * @type {Map<string, URL>}
     */
    const parsedURLs = new SafeMap();

    /**
     * Resolves a valid url string against the manifest
     * @param {string} originalHREF
     * @returns {string}
     */
    const resolve = (originalHREF) => {
      if (parsedURLs.has(originalHREF)) {
        return parsedURLs.get(originalHREF).href;
      } else if (
        RegExpPrototypeTest(kRelativeURLStringPattern, originalHREF)
      ) {
        const resourceURL = new URL(originalHREF, manifestURL);
        const resourceHREF = resourceURL.href;
        parsedURLs.set(originalHREF, resourceURL);
        parsedURLs.set(resourceURL.href, resourceURL);
        return resourceHREF;
      }
      const resourceURL = new URL(originalHREF);
      const resourceHREF = resourceURL.href;
      parsedURLs.set(originalHREF, resourceURL);
      return resourceHREF;
    };

    /**
     * @param {string} resourceHREF
     * @param {DependencyMap} dependencyMap
     * @param {boolean} cascade
     * @param {Map<string, DependencyMapper>} store
     */
    const insertDependencyMap = (
      resourceHREF,
      dependencyMap,
      cascade,
      store
    ) => {
      if (cascade !== undefined && typeof cascade !== 'boolean') {
        throw new ERR_MANIFEST_INVALID_RESOURCE_FIELD(
          resourceHREF,
          'cascade');
      }
      if (dependencyMap === null || dependencyMap === undefined) {
        dependencyMap = ObjectCreate(null);
      }
      if (
        typeof dependencyMap === 'object' &&
        !ArrayIsArray(dependencyMap)
      ) {
        const dependencyRedirectList = createDependencyMapper(
          resourceHREF,
          dependencyMap,
          cascade);
        store.set(resourceHREF, dependencyRedirectList);
        return;
      } else if (dependencyMap === true) {
        const arbitraryDependencies = /** @type {()=>true} */() => true;
        store.set(resourceHREF, arbitraryDependencies);
        return;
      }
      throw new ERR_MANIFEST_INVALID_RESOURCE_FIELD(
        resourceHREF,
        'dependencies');
    };
    /**
     * Does a special allowance for scopes to be non-valid URLs
     * that are only protocol strings
     * @param {string} resourceHREF
     * @returns {string}
     */
    const protocolOrResolve = (resourceHREF) => {
      if (StringPrototypeEndsWith(resourceHREF, ':')) {
        // URL parse will trim these anyway, save the compute
        resourceHREF = StringPrototypeReplace(
          resourceHREF,
          // eslint-disable-next-line
          /^[\x00-\x1F\x20]|\x09\x0A\x0D|[\x00-\x1F\x20]$/g,
          ''
        );
        if (RegExpPrototypeTest(/^[a-zA-Z][a-zA-Z+\-.]*:$/, resourceHREF)) {
          return resourceHREF;
        }
      }
      return resolve(resourceHREF);
    };

    for (let i = 0; i < jsonResourcesEntries.length; i++) {
      const { 0: originalHREF, 1: resourceDescriptor } =
        jsonResourcesEntries[i];
      const cascade = resourceDescriptor.cascade;
      const dependencyMap = resourceDescriptor.dependencies;
      const resourceHREF = resolve(originalHREF);

      const integrity = resourceDescriptor.integrity;
      if (typeof integrity !== 'undefined') {
        debug('Manifest contains integrity for resource %s', originalHREF);
        if (integrities.has(resourceHREF)) {
          if (integrities.get(resourceHREF) !== integrity) {
            throw new ERR_MANIFEST_INTEGRITY_MISMATCH(resourceHREF);
          }
        }
        if (typeof integrity === 'string') {
          integrities.set(resourceHREF, integrity);
        } else if (integrity === true) {
          integrities.set(resourceHREF, true);
        } else {
          throw new ERR_MANIFEST_INVALID_RESOURCE_FIELD(
            resourceHREF,
            'integrity');
        }
      } else {
        integrities.set(resourceHREF, cascade ? kCascade : null);
      }
      insertDependencyMap(resourceHREF, dependencyMap, cascade, dependencies);
    }

    const scopeIntegrities = this.#scopeIntegrities;
    for (let i = 0; i < jsonScopesEntries.length; i++) {
      const { 0: originalHREF, 1: scopeDescriptor } = jsonScopesEntries[i];
      const integrity = scopeDescriptor.integrity;
      const cascade = scopeDescriptor.cascade;
      const dependencyMap = scopeDescriptor.dependencies;
      const resourceHREF = protocolOrResolve(originalHREF);
      if (typeof integrity !== 'undefined') {
        debug('Manifest contains integrity for scope %s', originalHREF);
        if (scopeIntegrities.has(resourceHREF)) {
          if (scopeIntegrities.get(resourceHREF) !== integrity) {
            throw new ERR_MANIFEST_INTEGRITY_MISMATCH(resourceHREF);
          }
        }
        if (integrity === true) {
          scopeIntegrities.set(resourceHREF, true);
        } else {
          throw new ERR_MANIFEST_INVALID_RESOURCE_FIELD(
            resourceHREF,
            'integrity');
        }
      } else {
        scopeIntegrities.set(resourceHREF, cascade ? kCascade : null);
      }
      insertDependencyMap(resourceHREF, dependencyMap, cascade, scopes);
    }

    ObjectFreeze(this);
  }

  /**
   * Finds the longest key within `this.#scopeDependencies` that covers a
   * specific HREF
   * @param {string} href
   * @returns {null | string}
   */
  #findScopeHREF = (href) => {
    let currentURL = new URL(href);
    let protocol = currentURL.protocol;
    // Non-opaque blobs adopt origins
    if (protocol === 'blob:' && currentURL.origin !== 'null') {
      currentURL = new URL(currentURL.origin);
      protocol = currentURL.protocol;
    }
    // Only a few schemes are hierarchical
    if (kSpecialSchemes.has(currentURL.protocol)) {
      // Make first '..' act like '.'
      if (!StringPrototypeEndsWith(currentURL.pathname, '/')) {
        currentURL.pathname += '/';
      }
      let lastHREF;
      let currentHREF = currentURL.href;
      do {
        if (this.#scopeDependencies.has(currentHREF)) {
          return currentHREF;
        }
        lastHREF = currentHREF;
        currentURL = new URL('..', currentURL);
        currentHREF = currentURL.href;
      } while (lastHREF !== currentHREF);
    }
    if (this.#scopeDependencies.has(protocol)) {
      return protocol;
    }
    return null;
  }

  #createResolver = (resolve) => {
    return {
      resolve: (to, conditions) => resolve(`${to}`, conditions),
      reaction: this.#reaction
    };
  }

  /**
   * @param {string} requester
   */
  getDependencyMapper(requester) {
    const requesterHREF = `${requester}`;
    const dependencies = this.#resourceDependencies;
    if (dependencies.has(requesterHREF)) {
      return this.#createResolver(
        dependencies.get(requesterHREF) ||
        (() => null)
      );
    }
    const scopes = this.#scopeDependencies;
    if (scopes.size !== 0) {
      const scopeHREF = this.#findScopeHREF(requesterHREF);
      if (typeof scopeHREF === 'string') {
        return this.#createResolver(scopes.get(scopeHREF));
      }
    }
    return this.#createResolver(() => null);
  }

  assertIntegrity(url, content) {
    const href = `${url}`;
    debug('Checking integrity of %s', href);
    const realIntegrities = new SafeMap();
    const integrities = this.#resourceIntegrities;
    function processEntry(href) {
      let integrityEntries = integrities.get(href);
      if (integrityEntries === true) return true;
      if (typeof integrityEntries === 'string') {
        const sri = ObjectFreeze(SRI.parse(integrityEntries));
        integrities.set(href, sri);
        integrityEntries = sri;
      }
      return integrityEntries;
    }
    if (integrities.has(href)) {
      const integrityEntries = processEntry(href);
      if (integrityEntries === true) return true;
      if (ArrayIsArray(integrityEntries)) {
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
          realIntegrities.set(
            algorithm,
            BufferToString(digest, 'base64')
          );
        }
      }

      if (integrityEntries !== kCascade) {
        const error = new ERR_MANIFEST_ASSERT_INTEGRITY(url, realIntegrities);
        this.#reaction(error);
      }
    }
    let scope = this.#findScopeHREF(href);
    while (scope !== null) {
      if (this.#scopeIntegrities.has(scope)) {
        const entry = this.#scopeIntegrities.get(scope);
        if (entry === true) {
          return true;
        } else if (entry === kCascade) {
        } else {
          break;
        }
      }
      const nextScope = this.#findScopeHREF(new URL('..', scope));
      if (!nextScope || nextScope === scope) {
        break;
      }
      scope = nextScope;
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
