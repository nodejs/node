'use strict';
const { isURL, URL } = require('internal/url');
const {
  ObjectEntries,
  ObjectKeys,
  SafeMap,
  ArrayIsArray,
  StringPrototypeStartsWith,
  StringPrototypeEndsWith,
  StringPrototypeSlice,
  ArrayPrototypeReverse,
  ArrayPrototypeSort,
} = primordials;
const { codes: { ERR_INVALID_IMPORT_MAP } } = require('internal/errors');
const { shouldBeTreatedAsRelativeOrAbsolutePath } = require('internal/modules/helpers');

class ImportMap {
  #baseURL;
  #imports = new SafeMap();
  #scopes = new SafeMap();
  #specifiers = new SafeMap()

  /**
   * Process a raw import map object
   * @param {object} raw The plain import map object read from JSON file
   * @param {URL} baseURL The url to resolve relative to
   */
  constructor(raw, baseURL) {
    this.#baseURL = baseURL;
    this.process(raw);
  }

  // These are convenience methods mostly for tests
  get baseURL() {
    return this.#baseURL;
  }

  get imports() {
    return this.#imports;
  }

  get scopes() {
    return this.#scopes;
  }

  /**
   * Cache for mapped specifiers
   * @param {string | URL} originalSpecifier The original specifier being mapped
   */
  #getMappedSpecifier(originalSpecifier) {
    let mappedSpecifier = this.#specifiers.get(originalSpecifier);

    // Specifiers are processed and cached in this.#specifiers
    if (!mappedSpecifier) {
      // Try processing as a url, fall back for bare specifiers
      try {
        if (shouldBeTreatedAsRelativeOrAbsolutePath(originalSpecifier)) {
          mappedSpecifier = new URL(originalSpecifier, this.#baseURL);
        } else {
          mappedSpecifier = new URL(originalSpecifier);
        }
      } catch {
        // Ignore exception
        mappedSpecifier = originalSpecifier;
      }
      this.#specifiers.set(originalSpecifier, mappedSpecifier);
    }
    return mappedSpecifier;
  }

  /**
   * Resolve the module according to the import map.
   * @param {string | URL} specifier The specified URL of the module to be resolved.
   * @param {string | URL} [parentURL] The URL path of the module's parent.
   * @returns {string | URL} The resolved module specifier
   */
  resolve(specifier, parentURL = this.#baseURL) {
    // When using the customized loader the parent
    // will be a string (for transferring to the worker)
    // so just handle that here
    if (!isURL(parentURL)) {
      parentURL = new URL(parentURL);
    }

    // Process scopes
    for (const { 0: prefix, 1: mapping } of this.#scopes) {
      const _mappedSpecifier = mapping.get(specifier);
      if (StringPrototypeStartsWith(parentURL.pathname, prefix.pathname) && _mappedSpecifier) {
        const mappedSpecifier = this.#getMappedSpecifier(_mappedSpecifier);
        if (mappedSpecifier !== _mappedSpecifier) {
          mapping.set(specifier, mappedSpecifier);
        }
        specifier = mappedSpecifier;
        break;
      }
    }

    // Handle bare specifiers with sub paths
    let spec = specifier;
    let slashIndex = (typeof specifier === 'string' && specifier.indexOf('/')) || -1;
    let subSpec;
    let bareSpec;
    if (isURL(spec)) {
      spec = spec.href;
    } else if (slashIndex !== -1) {
      slashIndex += 1;
      subSpec = StringPrototypeSlice(spec, slashIndex);
      bareSpec = StringPrototypeSlice(spec, 0, slashIndex);
    }

    let _mappedSpecifier = this.#imports.get(bareSpec) || this.#imports.get(spec);
    if (_mappedSpecifier) {
      // Re-assemble sub spec
      if (_mappedSpecifier === spec && subSpec) {
        _mappedSpecifier += subSpec;
      }
      const mappedSpecifier = this.#getMappedSpecifier(_mappedSpecifier);

      if (mappedSpecifier !== _mappedSpecifier) {
        this.imports.set(specifier, mappedSpecifier);
      }
      specifier = mappedSpecifier;
    }

    return specifier;
  }

  /**
   * Process a raw import map object
   * @param {object} raw The plain import map object read from JSON file
   */
  process(raw) {
    if (!raw) {
      throw new ERR_INVALID_IMPORT_MAP('top level must be a plain object');
    }

    // Validation and normalization
    if (raw.imports === null || typeof raw.imports !== 'object' || ArrayIsArray(raw.imports)) {
      throw new ERR_INVALID_IMPORT_MAP('top level key "imports" is required and must be a plain object');
    }
    if (raw.scopes === null || typeof raw.scopes !== 'object' || ArrayIsArray(raw.scopes)) {
      throw new ERR_INVALID_IMPORT_MAP('top level key "scopes" is required and must be a plain object');
    }

    // Normalize imports
    const importsEntries = ObjectEntries(raw.imports);
    for (let i = 0; i < importsEntries.length; i++) {
      const { 0: specifier, 1: mapping } = importsEntries[i];
      if (!specifier || typeof specifier !== 'string') {
        throw new ERR_INVALID_IMPORT_MAP('module specifier keys must be non-empty strings');
      }
      if (!mapping || typeof mapping !== 'string') {
        throw new ERR_INVALID_IMPORT_MAP('module specifier values must be non-empty strings');
      }
      if (StringPrototypeEndsWith(specifier, '/') && !StringPrototypeEndsWith(mapping, '/')) {
        throw new ERR_INVALID_IMPORT_MAP('module specifier keys ending with "/" must have values that end with "/"');
      }

      this.imports.set(specifier, mapping);
    }

    // Normalize scopes
    // Sort the keys according to spec and add to the map in order
    // which preserves the sorted map requirement
    const sortedScopes = ArrayPrototypeReverse(ArrayPrototypeSort(ObjectKeys(raw.scopes)));
    for (let i = 0; i < sortedScopes.length; i++) {
      let scope = sortedScopes[i];
      const _scopeMap = raw.scopes[scope];
      if (!scope || typeof scope !== 'string') {
        throw new ERR_INVALID_IMPORT_MAP('import map scopes keys must be non-empty strings');
      }
      if (!_scopeMap || typeof _scopeMap !== 'object') {
        throw new ERR_INVALID_IMPORT_MAP(`scope values must be plain objects (${scope} is ${typeof _scopeMap})`);
      }

      // Normalize scope
      scope = new URL(scope, this.#baseURL);

      const scopeMap = new SafeMap();
      const scopeEntries = ObjectEntries(_scopeMap);
      for (let i = 0; i < scopeEntries.length; i++) {
        const { 0: specifier, 1: mapping } = scopeEntries[i];
        if (StringPrototypeEndsWith(specifier, '/') && !StringPrototypeEndsWith(mapping, '/')) {
          throw new ERR_INVALID_IMPORT_MAP('module specifier keys ending with "/" must have values that end with "/"');
        }
        scopeMap.set(specifier, mapping);
      }

      this.scopes.set(scope, scopeMap);
    }
  }
}

module.exports = {
  ImportMap,
};
