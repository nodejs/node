'use strict';

const {
  ArrayIsArray,
  ArrayPrototypeMap,
  ArrayPrototypePush,
} = primordials;

const { lstatSync } = require('fs');
const { basename, dirname, isAbsolute, join, resolve } = require('path');

const {
  kEmptyObject,
  isWindows,
} = require('internal/util');
const {
  validateBoolean,
  validateInteger,
  validateObject,
  validateString,
  validateStringArray,
} = require('internal/validators');
const { Dirent, DirentFromStats } = require('internal/fs/utils');
const {
  codes: {
    ERR_INVALID_ARG_TYPE,
  },
  hideStackFrames,
} = require('internal/errors');
const { toPathIfFileURL } = require('internal/url');
const globBinding = internalBinding('glob');

// Flag bits defined by the native engine (src/glob/node_glob.cc), which
// applies the host platform's own bits itself.
const {
  kFlagWindows,
  kFlagDot,
  kFlagFollowSymlinks,
  kFlagWithFileTypes,
  kFlagMatchBase,
} = globBinding;

/**
 * @param {string} path
 * @returns {DirentFromStats|null}
 */
function getDirentSync(path) {
  let stat;
  try {
    stat = lstatSync(path);
  } catch {
    return null;
  }
  return new DirentFromStats(basename(path), stat, dirname(path));
}

/**
 * @callback validateStringArrayOrFunction
 * @param {*} value
 * @param {string} name
 */
const validateStringArrayOrFunction = hideStackFrames((value, name) => {
  if (ArrayIsArray(value)) {
    validateStringArray(value, name);
    return;
  }
  if (typeof value !== 'function') {
    throw new ERR_INVALID_ARG_TYPE(name, ['string[]', 'function'], value);
  }
});

class Glob {
  #root;
  #patterns;
  #excludePatterns;
  #excludeFn;
  #withFileTypes;
  #followSymlinks = false;
  #maxDepth = Infinity;

  constructor(pattern, options = kEmptyObject) {
    validateObject(options, 'options');
    const { exclude, cwd, followSymlinks, maxDepth, withFileTypes } = options;
    this.#root = toPathIfFileURL(cwd) ?? '.';
    validateString(this.#root, 'options.cwd');
    if (followSymlinks != null) {
      validateBoolean(followSymlinks, 'options.followSymlinks');
      this.#followSymlinks = followSymlinks;
    }
    if (maxDepth != null && maxDepth !== Infinity) {
      validateInteger(maxDepth, 'options.maxDepth', 0);
      this.#maxDepth = maxDepth;
    }
    this.#withFileTypes = !!withFileTypes;
    this.#excludePatterns = [];
    if (exclude != null) {
      validateStringArrayOrFunction(exclude, 'options.exclude');
      if (ArrayIsArray(exclude)) {
        // Exclude patterns are matched against absolute paths. Exclude
        // matching is a pure string comparison with no filesystem lookups, so
        // unlike include patterns, literal segments also match
        // case-insensitively on case-insensitive platforms (see native).
        this.#excludePatterns = ArrayPrototypeMap(
          exclude, (pattern) => resolve(this.#root, pattern));
      } else {
        this.#excludeFn = exclude;
      }
    }
    if (typeof pattern === 'object') {
      validateStringArray(pattern, 'patterns');
      this.#patterns = pattern;
    } else {
      validateString(pattern, 'patterns');
      this.#patterns = [pattern];
    }
  }

  // Whether any pattern is more than a literal path, see native.
  get hasMagic() {
    for (let i = 0; i < this.#patterns.length; i++) {
      if (globBinding.hasMagic(this.#patterns[i])) return true;
    }
    return false;
  }

  globSync() {
    const result = globBinding.globSync(
      this.#root, this.#patterns, this.#excludePatterns, this.#flags(),
      this.#excludeAdapter(), this.#maxDepth);
    if (!this.#withFileTypes) {
      return result;
    }
    const { 0: paths, 1: types } = result;
    const dirents = [];
    for (let i = 0; i < paths.length; i++) {
      ArrayPrototypePush(dirents, this.#toDirent(paths[i], types[i]));
    }
    return dirents;
  }

  async globAll() {
    const handle = globBinding.globStart(
      this.#root, this.#patterns, this.#excludePatterns, this.#flags(),
      this.#excludeAdapter(), this.#maxDepth);
    try {
      const { 0: paths, 1: types } = await handle.all();
      if (!this.#withFileTypes) {
        return paths;
      }
      const dirents = [];
      for (let i = 0; i < paths.length; i++) {
        ArrayPrototypePush(dirents, this.#toDirent(paths[i], types[i]));
      }
      return dirents;
    } finally {
      handle.cancel();
    }
  }

  // Pulls one batch of results at a time
  async * glob() {
    const handle = globBinding.globStart(
      this.#root, this.#patterns, this.#excludePatterns, this.#flags(),
      this.#excludeAdapter(), this.#maxDepth);
    try {
      let done = false;
      while (!done) {
        const { 0: paths, 1: types, 2: batchDone } = await handle.next();
        done = batchDone;
        for (let i = 0; i < paths.length; i++) {
          yield this.#withFileTypes ? this.#toDirent(paths[i], types[i]) : paths[i];
        }
      }
    } finally {
      handle.cancel();
    }
  }

  #flags() {
    return (this.#followSymlinks ? kFlagFollowSymlinks : 0) |
      (this.#withFileTypes ? kFlagWithFileTypes : 0);
  }

  // Shapes the argument an `exclude` callback receives to
  // work both with and without Dirents
  #excludeAdapter() {
    const exclude = this.#excludeFn;
    if (exclude === undefined) {
      return undefined;
    }
    if (!this.#withFileTypes) {
      return (entry, value) => exclude(value);
    }
    return (entry, value, parentPath, type) => {
      if (entry) {
        return exclude(new Dirent(value, type, parentPath));
      }
      const dirent = getDirentSync(value);
      return dirent === null ? false : exclude(dirent);
    };
  }

  #toDirent(path, type) {
    const full = isAbsolute(path) ? path : join(this.#root, path);
    return new Dirent(basename(full), type, dirname(full));
  }
}

/**
 * Check if a path matches a glob pattern
 * @param {string} path the path to check
 * @param {string} pattern the glob pattern to match
 * @param {boolean} windows whether the path is on a Windows system, defaults to `isWindows`
 * @param {boolean} dot whether wildcards may match dotfiles, defaults to `false`
 * @returns {boolean}
 */
function matchGlobPattern(path, pattern, windows = isWindows, dot = false) {
  validateString(path, 'path');
  validateString(pattern, 'pattern');
  return globBinding.matchesGlob(
    path, pattern, (windows ? kFlagWindows : 0) | (dot ? kFlagDot : 0));
}

/**
 * Check if a path matches a glob pattern, letting a pattern without a
 * separator match the path's basename
 * @param {string} path the path to check
 * @param {string} pattern the glob pattern to match
 * @returns {boolean}
 */
function matchGlobBasename(path, pattern) {
  return globBinding.matchesGlob(
    path, pattern, (isWindows ? kFlagWindows : 0) | kFlagMatchBase);
}

module.exports = {
  __proto__: null,
  Glob,
  matchGlobBasename,
  matchGlobPattern,
};
