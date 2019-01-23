'use strict';

const {
  ERR_MANIFEST_TDZ,
} = require('internal/errors').codes;
const { Manifest } = require('internal/policy/manifest');
let manifest;
let manifestSrc;
let manifestURL;

module.exports = Object.freeze({
  __proto__: null,
  setup(src, url) {
    manifestSrc = src;
    manifestURL = url;
    if (src === null) {
      manifest = null;
      return;
    }

    const json = JSON.parse(src, (_, o) => {
      if (o && typeof o === 'object') {
        Reflect.setPrototypeOf(o, null);
        Object.freeze(o);
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
    this.manifest.matchesIntegrity(moduleURL, content);
  }
});
