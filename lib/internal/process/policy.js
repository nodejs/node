'use strict';

const {
  JSONParse,
  ObjectFreeze,
  ReflectSetPrototypeOf,
  SafeMap,
} = primordials;

const {
  ERR_MANIFEST_TDZ,
  ERR_MANIFEST_ASSERT_INTEGRITY,
} = require('internal/errors').codes;
const { Manifest } = require('internal/policy/manifest');
const { pathToFileURL, URL } = require('internal/url');
const { readFileSync } = require('fs');
const { isAbsolute } = require('path');
const { parse } = require('internal/policy/sri');

let manifest;
let manifestSrc;
let manifestURL;

module.exports = ObjectFreeze({
  __proto__: null,
  setup(policyPath, policyIntegrity) {
    // URL here as it is slightly different parsing
    // no bare specifiers for now
    let manifestURLObject;
    if (isAbsolute(policyPath)) {
      manifestURLObject = new URL(`file://${policyPath}`);
    } else {
      const cwdURL = pathToFileURL(process.cwd());
      cwdURL.pathname += '/';
      manifestURLObject = new URL(policyPath, cwdURL);
    }
    manifestSrc = readFileSync(manifestURLObject, 'utf8');

    if (policyIntegrity) {
      // These should be loaded lazily in case the build does not have crypto.
      const { createHash, timingSafeEqual } = require('crypto');
      const realIntegrities = new SafeMap();
      const integrityEntries = parse(policyIntegrity);
      let foundMatch = false;
      for (let i = 0; i < integrityEntries.length; i++) {
        const {
          algorithm,
          value: expected
        } = integrityEntries[i];
        const hash = createHash(algorithm);
        hash.update(manifestSrc);
        const digest = hash.digest();
        if (digest.length === expected.length &&
          timingSafeEqual(digest, expected)) {
          foundMatch = true;
          break;
        }
        realIntegrities.set(algorithm, digest.toString('base64'));
      }
      if (!foundMatch) {
        throw new ERR_MANIFEST_ASSERT_INTEGRITY(manifestURL, realIntegrities);
      }
    }
    manifestURL = manifestURLObject.href;

    if (manifestSrc === null) {
      manifest = null;
      return;
    }

    const json = JSONParse(manifestSrc, (_, o) => {
      if (o && typeof o === 'object') {
        ReflectSetPrototypeOf(o, null);
        ObjectFreeze(o);
      }
      return o;
    });
    manifest = new Manifest(json, manifestURL);
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
