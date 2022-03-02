'use strict';

const {
  JSONParse,
  ObjectFreeze,
  ReflectSetPrototypeOf,
} = primordials;

const {
  ERR_MANIFEST_TDZ,
} = require('internal/errors').codes;
const { Manifest } = require('internal/policy/manifest');
/**
 * @type {InstanceType<Manifest>}
 */
let manifest;
/**
 * @type {string}
 */
let manifestSrc;
/**
 * @type {string}
 */
let manifestURL;

module.exports = ObjectFreeze({
  __proto__: null,
  /**
   * @param {string | null} src null to not use a policy
   * @param {string} url
   * @returns {void}
   */
  setup(src, url) {
    manifestSrc = src;
    manifestURL = url;
    if (src === null) {
      manifest = null;
      manifestSrc = null;
      manifestURL = null;
      return;
    }

    const json = JSONParse(src, (_, o) => {
      if (o && typeof o === 'object') {
        ReflectSetPrototypeOf(o, null);
        ObjectFreeze(o);
      }
      return o;
    });
    manifest = new Manifest(json, url);
  },

  get manifest() {
    if (typeof manifest === 'undefined') {
      throw new ERR_MANIFEST_TDZ();
    }
    return manifest;
  },

  get src() {
    if (typeof manifestSrc === 'undefined') {
      throw new ERR_MANIFEST_TDZ();
    }
    return manifestSrc;
  },

  get url() {
    if (typeof manifestURL === 'undefined') {
      throw new ERR_MANIFEST_TDZ();
    }
    return manifestURL;
  },

  assertIntegrity(moduleURL, content) {
    this.manifest.assertIntegrity(moduleURL, content);
  }
});
