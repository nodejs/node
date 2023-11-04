'use strict';

const {
  ObjectDefineProperties,
  StringPrototypeIndexOf,
  StringPrototypeSlice,
  StringPrototypeToUpperCase,
  ObjectFreeze,
  globalThis,
  Symbol,
} = primordials;

const {
  Intl,
} = globalThis;

const {
  ERR_ILLEGAL_CONSTRUCTOR,
} = require('internal/errors').codes;

const {
  kEnumerableProperty,
} = require('internal/util');

const {
  getAvailableParallelism,
} = internalBinding('os');

const kInitialize = Symbol('kInitialize');
const nodeVersion = process.version;

/**
 * @param {object} process
 * @param {string} process.platform
 * @param {string} process.arch
 * @returns {string}
 */
function getNavigatorPlatform(process) {
  if (process.platform === 'darwin') {
    // On macOS, modern browsers return 'MacIntel' even if running on Apple Silicon.
    return 'MacIntel';
  } else if (process.platform === 'win32') {
    // On Windows, modern browsers return 'Win32' even if running on a 64-bit version of Windows.
    // https://developer.mozilla.org/en-US/docs/Web/API/Navigator/platform#usage_notes
    return 'Win32';
  } else if (process.platform === 'linux') {
    if (process.arch === 'ia32') {
      return 'Linux i686';
    } else if (process.arch === 'x64') {
      return 'Linux x86_64';
    }
    return `Linux ${process.arch}`;
  } else if (process.platform === 'freebsd') {
    if (process.arch === 'ia32') {
      return 'FreeBSD i386';
    } else if (process.arch === 'x64') {
      return 'FreeBSD amd64';
    }
    return `FreeBSD ${process.arch}`;
  } else if (process.platform === 'openbsd') {
    if (process.arch === 'ia32') {
      return 'OpenBSD i386';
    } else if (process.arch === 'x64') {
      return 'OpenBSD amd64';
    }
    return `OpenBSD ${process.arch}`;
  } else if (process.platform === 'sunos') {
    if (process.arch === 'ia32') {
      return 'SunOS i86pc';
    }
    return `SunOS ${process.arch}`;
  } else if (process.platform === 'aix') {
    return 'AIX';
  }
  return `${StringPrototypeToUpperCase(process.platform[0])}${StringPrototypeSlice(process.platform, 1)} ${process.arch}`;
}

class Navigator {
  // Private properties are used to avoid brand validations.
  #availableParallelism;
  #userAgent = `Node.js/${StringPrototypeSlice(nodeVersion, 1, StringPrototypeIndexOf(nodeVersion, '.'))}`;
  #platform = getNavigatorPlatform(process);
  #language = Intl?.Collator().resolvedOptions().locale || 'en-US';
  #languages = ObjectFreeze([this.#language]);

  constructor() {
    if (arguments[0] === kInitialize) {
      return;
    }
    throw new ERR_ILLEGAL_CONSTRUCTOR();
  }

  /**
   * @return {number}
   */
  get hardwareConcurrency() {
    this.#availableParallelism ??= getAvailableParallelism();
    return this.#availableParallelism;
  }

  /**
   * @return {string}
   */
  get language() {
    return this.#language;
  }

  /**
   * @return {Array<string>}
   */
  get languages() {
    return this.#languages;
  }

  /**
   * @return {string}
   */
  get userAgent() {
    return this.#userAgent;
  }

  /**
   * @return {string}
   */
  get platform() {
    return this.#platform;
  }
}

ObjectDefineProperties(Navigator.prototype, {
  hardwareConcurrency: kEnumerableProperty,
  language: kEnumerableProperty,
  languages: kEnumerableProperty,
  userAgent: kEnumerableProperty,
  platform: kEnumerableProperty,
});

module.exports = {
  getNavigatorPlatform,
  navigator: new Navigator(kInitialize),
  Navigator,
};
