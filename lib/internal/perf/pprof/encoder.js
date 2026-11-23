// Copyright 2019 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Modified for inclusion in Node.js core: ported from
// https://github.com/DataDog/pprof-nodejs/blob/3905f382723fc2554284e1abbba2df0f2dc6f82b/ts/src/profile-encoder.ts
//
// SPDX-License-Identifier: Apache-2.0

'use strict';

const {
  Promise,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
  },
} = require('internal/errors');
const { Profile } = require('internal/perf/pprof/profile');

// The zlib module is only loaded when a profile is encoded, so that loading
// node:perf_hooks/pprof to profile does not load it.
let zlib;
/**
 * Loads the zlib module the first time it is needed.
 * @returns {object} The zlib module.
 */
function lazyZlib() {
  zlib ??= require('zlib');
  return zlib;
}

/**
 * Validates the argument of `encode()` and `encodeSync()`, so that they only
 * encode profiles that the time profiler returned, or that were built from
 * the same `Profile` class.
 * @param {any} profile The value to validate.
 * @throws {TypeError} An `ERR_INVALID_ARG_TYPE` error if `profile` is not a
 *   `pprof.Profile`.
 */
function validateProfile(profile) {
  if (!(profile instanceof Profile)) {
    throw new ERR_INVALID_ARG_TYPE('profile', 'pprof.Profile', profile);
  }
}

/**
 * Encodes a profile as a gzip-compressed pprof protobuf message, yielding to
 * the event loop while encoding.
 * @param {Profile} profile The profile to encode.
 * @returns {Promise<Buffer>} The gzip-compressed encoding of the profile.
 * @throws {TypeError} Rejects with an `ERR_INVALID_ARG_TYPE` error if
 *   `profile` is not a `pprof.Profile`.
 * @throws {Error} Rejects with the error of zlib if compressing fails.
 */
async function encode(profile) {
  validateProfile(profile);
  const { gzip } = lazyZlib();
  const encoded = await profile.encodeAsync();
  return new Promise((resolve, reject) => {
    gzip(encoded, (err, result) => {
      if (err) reject(err);
      else resolve(result);
    });
  });
}

/**
 * Encodes a profile as a gzip-compressed pprof protobuf message.
 * @param {Profile} profile The profile to encode.
 * @returns {Buffer} The gzip-compressed encoding of the profile.
 * @throws {TypeError} An `ERR_INVALID_ARG_TYPE` error if `profile` is not a
 *   `pprof.Profile`.
 * @throws {Error} The error of zlib if compressing fails.
 */
function encodeSync(profile) {
  validateProfile(profile);
  return lazyZlib().gzipSync(profile.encode());
}

module.exports = {
  encode,
  encodeSync,
};
