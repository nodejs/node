'use strict';

const {
  Symbol,
} = primordials;

const {
  IsolateGroup: IsolateGroupHandle,
  isolateGroupsSupported,
} = internalBinding('worker');
const {
  codes: {
    ERR_ISOLATE_GROUPS_UNSUPPORTED,
  },
} = require('internal/errors');

const kCurrent = Symbol('kCurrent');

let currentIsolateGroup;
let getHandle;

/**
 * A V8 isolate group. The isolates in a group share a pointer compression
 * cage, which limits their heaps to 4 GB together, and the read-only part of
 * their heaps.
 */
class IsolateGroup {
  #handle;

  constructor(token = undefined) {
    if (token === kCurrent) {
      this.#handle = new IsolateGroupHandle(true);
      return;
    }
    if (!isolateGroupsSupported) {
      throw new ERR_ISOLATE_GROUPS_UNSUPPORTED();
    }
    this.#handle = new IsolateGroupHandle(false);
  }

  /**
   * The isolate group of the current thread.
   * @type {IsolateGroup}
   */
  static get current() {
    currentIsolateGroup ??= new IsolateGroup(kCurrent);
    return currentIsolateGroup;
  }

  /**
   * Whether this build of Node.js can create isolate groups.
   * @type {boolean}
   */
  static get supported() {
    return isolateGroupsSupported;
  }

  static {
    getHandle = (value) => (#handle in value ? value.#handle : undefined);
  }
}

/**
 * @param {any} value
 * @returns {object|undefined} The handle of `value` if it is an IsolateGroup.
 */
function getIsolateGroupHandle(value) {
  if (typeof value !== 'object' || value === null) return undefined;
  return getHandle(value);
}

module.exports = {
  IsolateGroup,
  getIsolateGroupHandle,
};
