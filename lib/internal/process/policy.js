'use strict';

const {
  JSONParse,
  ObjectFreeze,
  ReflectSetPrototypeOf,
} = primordials;

const {
  ERR_MANIFEST_TDZ,
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_ARG_VALUE,
} = require('internal/errors').codes;

const policy = internalBinding('policy');

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

  deny(permissions) {
    if (typeof permissions !== 'string')
      throw new ERR_INVALID_ARG_TYPE('permissions', 'string', permissions);
    const ret = policy.deny(permissions);
    if (ret === undefined)
      throw new ERR_INVALID_ARG_VALUE('permissions', permissions);
  },

  fastCheck(permission) {
    // This should only be used by internal code. Skips explicit
    // type checking to improve performance. The permission
    // argument must be a Int32
    return policy.fastCheck(permission);
  },

  check(permissions) {
    if (typeof permissions !== 'string')
      throw new ERR_INVALID_ARG_TYPE('permission', 'string', permissions);
    const ret = policy.check(permissions);
    if (ret === undefined)
      throw new ERR_INVALID_ARG_VALUE('permissions', permissions);
    return ret;
  }
});
