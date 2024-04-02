'use strict';

const {
  JSONParse,
  ObjectFreeze,
  ReflectSetPrototypeOf,
} = primordials;

const {
  ERR_ACCESS_DENIED,
  ERR_MANIFEST_TDZ,
} = require('internal/errors').codes;
const { Manifest } = require('internal/policy/manifest');
let manifest;
let manifestSrc;
let manifestURL;

module.exports = ObjectFreeze({
  __proto__: null,
  setup(src, url) {
    manifestSrc = src;
    manifestURL = url;
    if (src === null) {
      manifest = null;
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

    // process.binding() is deprecated (DEP0111) and trivially allows bypassing
    // policies, so if policies are enabled, make this API unavailable.
    process.binding = function binding(_module) {
      throw new ERR_ACCESS_DENIED('process.binding');
    };
    process._linkedBinding = function _linkedBinding(_module) {
      throw new ERR_ACCESS_DENIED('process._linkedBinding');
    };
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
  },
});
