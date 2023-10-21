'use strict';
const { isURL, URL } = require('internal/url');
const {
  ObjectEntries,
  ObjectKeys,
  SafeMap,
  ArrayIsArray,
  StringPrototypeStartsWith,
  StringPrototypeEndsWith,
  ArrayPrototypeReverse,
  ArrayPrototypeSort
} = primordials;
const { codes: { ERR_INVALID_IMPORT_MAP } } = require('internal/errors');

class ImportMap {
  #baseURL;
  #imports = new SafeMap();
  #scopes = new SafeMap();

  constructor(raw, baseURL) {
    this.#baseURL = baseURL;
    this.process(raw, this.#baseURL);
  }

  // These are convinenince methods mostly for tests
  get baseURL() {
    return this.#baseURL;
  }

  get imports() {
    return this.#imports;
  }

  get scopes() {
    return this.#scopes;
  }

  resolve(specifier, parentURL = this.#baseURL) {
    // Process scopes
    for (const { 0: prefix, 1: mapping } of this.#scopes) {
      let mappedSpecifier = mapping.get(specifier);
      if (StringPrototypeStartsWith(parentURL.pathname, prefix.pathname) && mappedSpecifier) {
        if (!isURL(mappedSpecifier)) {
          mappedSpecifier = new URL(mappedSpecifier, this.#baseURL);
          mapping.set(specifier, mappedSpecifier);
        }
        specifier = mappedSpecifier;
        break;
      }
    }

    let spec = isURL(specifier) ? specifier.pathname : specifier;
    let importMapping = this.#imports.get(spec);
    if (importMapping) {
      if (!isURL(importMapping)) {
        importMapping = new URL(importMapping, this.#baseURL);
        this.imports.set(spec, importMapping);
      }
      return importMapping;
    }

    return specifier;
  }

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
