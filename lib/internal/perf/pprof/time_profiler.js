// Copyright 2017 Google Inc. All Rights Reserved.
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
// https://github.com/DataDog/pprof-nodejs/blob/3905f382723fc2554284e1abbba2df0f2dc6f82b/ts/src/time-profiler.ts
// without sampling contexts, source maps and labels.
//
// SPDX-License-Identifier: Apache-2.0

'use strict';

const {
  ObjectFreeze,
} = primordials;

const {
  WallProfiler,
  signalSamplingSupported,
} = internalBinding('pprof');
const {
  codes: {
    ERR_FEATURE_UNAVAILABLE_ON_PLATFORM,
    ERR_INCOMPATIBLE_OPTION_PAIR,
    ERR_INVALID_STATE,
    ERR_OUT_OF_RANGE,
  },
} = require('internal/errors');
const { kEmptyObject } = require('internal/util');
const {
  validateAbortSignal,
  validateBoolean,
  validateInteger,
  validateObject,
} = require('internal/validators');
const {
  throwIfBuildingSnapshot,
} = require('internal/v8/startup_snapshot');
const {
  GARBAGE_COLLECTION_FUNCTION_NAME,
  NON_JS_THREADS_FUNCTION_NAME,
} = require('internal/perf/pprof/normalizer');
const {
  serializeTimeProfile,
} = require('internal/perf/pprof/serializer');

const kMaxInt32 = 2 ** 31 - 1;
const kDefaultIntervalMicros = 1000;
const kDefaultDurationMillis = 60000;

// The state of the profiler of this thread.
let profiler;
let profilerIntervalMicros;
let stuckEventLoopDetected = 0;

/**
 * Validates the options of `start()`, and fills in their defaults.
 * @param {object} options The options passed to `start()`.
 * @param {number} [options.intervalMicros] The average time between samples,
 *   in microseconds.
 * @param {number} [options.durationMillis] The expected time between calls
 *   to `stop()`, in milliseconds, used to size buffers upfront.
 * @param {boolean} [options.lineNumbers] Whether to aggregate samples by line
 *   rather than by function.
 * @param {boolean} [options.collectCpuTime] Whether to attribute the thread's
 *   CPU time to samples.
 * @param {boolean} [options.workaroundV8Bug] Whether to work around the V8
 *   bug that can leave the profiler stuck when it is restarted.
 * @returns {{
 *   intervalMicros: number,
 *   durationMillis: number,
 *   lineNumbers: boolean,
 *   collectCpuTime: boolean,
 *   workaroundV8Bug: boolean,
 * }} The options, with the defaults of those that were not set.
 * @throws {TypeError} An `ERR_INVALID_ARG_TYPE` error if `options` or one of its
 *   properties has the wrong type.
 * @throws {RangeError} An `ERR_OUT_OF_RANGE` error if `intervalMicros` or
 *   `durationMillis` is out of range, or if `durationMillis` is shorter than
 *   `intervalMicros`.
 * @throws {TypeError} An `ERR_FEATURE_UNAVAILABLE_ON_PLATFORM` error if
 *   `collectCpuTime` is enabled on Windows.
 * @throws {TypeError} An `ERR_INCOMPATIBLE_OPTION_PAIR` error if
 *   `collectCpuTime` and `lineNumbers` are both enabled.
 */
function validateOptions(options) {
  validateObject(options, 'options');
  const {
    intervalMicros = kDefaultIntervalMicros,
    durationMillis = kDefaultDurationMillis,
    lineNumbers = false,
    collectCpuTime = false,
    workaroundV8Bug = true,
  } = options;
  validateInteger(intervalMicros, 'options.intervalMicros', 1, kMaxInt32);
  validateInteger(durationMillis, 'options.durationMillis', 1, kMaxInt32);
  if (durationMillis * 1000 < intervalMicros) {
    throw new ERR_OUT_OF_RANGE(
      'options.durationMillis',
      `>= options.intervalMicros / 1000 (${intervalMicros / 1000})`,
      durationMillis);
  }
  validateBoolean(lineNumbers, 'options.lineNumbers');
  validateBoolean(collectCpuTime, 'options.collectCpuTime');
  validateBoolean(workaroundV8Bug, 'options.workaroundV8Bug');
  if (collectCpuTime && !signalSamplingSupported) {
    throw new ERR_FEATURE_UNAVAILABLE_ON_PLATFORM('options.collectCpuTime');
  }
  if (collectCpuTime && lineNumbers) {
    throw new ERR_INCOMPATIBLE_OPTION_PAIR('collectCpuTime', 'lineNumbers');
  }
  return {
    intervalMicros,
    durationMillis,
    lineNumbers,
    collectCpuTime,
    workaroundV8Bug,
  };
}

/**
 * Checks that the profiler of this thread is started, before it is used.
 * @throws {Error} An `ERR_INVALID_STATE` error if the profiler of this thread
 *   is not started.
 */
function assertStarted() {
  if (profiler === undefined) {
    throw new ERR_INVALID_STATE('The time profiler is not started');
  }
}

/**
 * Updates the state of the profiler after it was stopped and restarted.
 *
 * Works around a V8 bug where the profiler's event processor thread gets
 * stuck in a loop eating 100% CPU, which leads to empty profiles: when the
 * restart found V8's CPU profiler stuck, the profiler is stopped, which
 * disposes of V8's CPU profiler, and started again with a new one. If that
 * start fails, the profiler is torn down, as when the profiler fails to
 * restart in the first place: the profile that was stopped is still valid,
 * and is returned rather than lost.
 */
function handleStopRestart() {
  stuckEventLoopDetected = profiler.v8ProfilerStuckEventLoopDetected();
  if (stuckEventLoopDetected > 0) {
    profiler.stop(false);
    try {
      profiler.start();
    } catch {
      handleStopNoRestart();
    }
  }
}

/**
 * Tears down the profiler of this thread after it was stopped without being
 * restarted.
 */
function handleStopNoRestart() {
  stuckEventLoopDetected = 0;
  profiler.dispose();
  profiler = undefined;
}

/**
 * Starts profiling the current thread.
 * @param {object} [options] Tuning parameters for the profiler.
 * @param {number} [options.intervalMicros] The average time between samples,
 *   in microseconds.
 * @param {number} [options.durationMillis] The expected time between calls
 *   to `stop()`, in milliseconds, used to size buffers upfront.
 * @param {boolean} [options.lineNumbers] Whether to aggregate samples by line
 *   rather than by function.
 * @param {boolean} [options.collectCpuTime] Whether to attribute the thread's
 *   CPU time to samples.
 * @param {boolean} [options.workaroundV8Bug] Whether to work around the V8
 *   bug that can leave the profiler stuck when it is restarted.
 * @throws {TypeError} An `ERR_INVALID_ARG_TYPE` error if `options` or one of its
 *   properties has the wrong type.
 * @throws {RangeError} An `ERR_OUT_OF_RANGE` error if `intervalMicros` or
 *   `durationMillis` is out of range, or if `durationMillis` is shorter than
 *   `intervalMicros`.
 * @throws {TypeError} An `ERR_FEATURE_UNAVAILABLE_ON_PLATFORM` error if
 *   `collectCpuTime` is enabled on Windows.
 * @throws {TypeError} An `ERR_INCOMPATIBLE_OPTION_PAIR` error if
 *   `collectCpuTime` and `lineNumbers` are both enabled.
 * @throws {Error} An `ERR_INVALID_STATE` error if the profiler of this thread
 *   is already started, or if the `SIGPROF` signal handler cannot be
 *   installed.
 * @throws {Error} An `ERR_NOT_SUPPORTED_IN_SNAPSHOT` error if a startup
 *   snapshot is being built.
 * @throws {Error} An `ERR_CPU_PROFILE_TOO_MANY` error if V8 has too many CPU
 *   profiles running.
 */
function start(options = kEmptyObject) {
  const {
    intervalMicros,
    durationMillis,
    lineNumbers,
    collectCpuTime,
    workaroundV8Bug,
  } = validateOptions(options);
  if (profiler !== undefined) {
    throw new ERR_INVALID_STATE('The time profiler is already started');
  }
  throwIfBuildingSnapshot('Starting the pprof time profiler');

  const newProfiler = new WallProfiler(intervalMicros,
                                       durationMillis,
                                       lineNumbers,
                                       workaroundV8Bug,
                                       collectCpuTime);
  newProfiler.start();
  profiler = newProfiler;
  profilerIntervalMicros = intervalMicros;
  stuckEventLoopDetected = 0;
}

/**
 * Stops the current profile, and returns it.
 * @param {boolean} [restart] Whether to start a new profile immediately, with
 *   the same options.
 * @returns {Profile} The profile that was stopped.
 * @throws {TypeError} An `ERR_INVALID_ARG_TYPE` error if `restart` is not a
 *   boolean.
 * @throws {Error} An `ERR_INVALID_STATE` error if the profiler of this thread
 *   is not started.
 */
function stop(restart = false) {
  validateBoolean(restart, 'restart');
  assertStarted();

  const profile = profiler.stop(restart);
  if (profile.restarted) {
    // The next profile already started: process this one after restarting
    // the profiler, so that the cost of processing it is captured in the next
    // profile.
    handleStopRestart();
    return serializeTimeProfile(profile, profilerIntervalMicros, true);
  }

  try {
    return serializeTimeProfile(profile, profilerIntervalMicros, true);
  } finally {
    // The native profiler has already stopped, so tear down even if
    // processing throws.
    handleStopNoRestart();
  }
}

/**
 * Profiles the current thread for `options.durationMillis` milliseconds.
 * @param {object} [options] Tuning parameters for the profiler, as for
 *   `start()`, and the signal that stops it early.
 * @param {number} [options.intervalMicros] The average time between samples,
 *   in microseconds.
 * @param {number} [options.durationMillis] The expected time between calls
 *   to `stop()`, in milliseconds, used to size buffers upfront.
 * @param {boolean} [options.lineNumbers] Whether to aggregate samples by line
 *   rather than by function.
 * @param {boolean} [options.collectCpuTime] Whether to attribute the thread's
 *   CPU time to samples.
 * @param {boolean} [options.workaroundV8Bug] Whether to work around the V8
 *   bug that can leave the profiler stuck when it is restarted.
 * @param {AbortSignal} [options.signal] Stops profiling, and rejects the
 *   returned promise, when aborted.
 * @returns {Promise<Profile>} The profile, once `options.durationMillis` has
 *   elapsed.
 * @throws {Error} Rejects with the errors that `start()` throws, with an
 *   `ERR_INVALID_ARG_TYPE` error if `options.signal` is not an `AbortSignal`,
 *   with an `AbortError` if `options.signal` is aborted, and with an
 *   `ERR_INVALID_STATE` error if the profiler that this call started was
 *   stopped, by `stop()` without restart, before `options.durationMillis`
 *   elapsed.
 */
async function profile(options = kEmptyObject) {
  validateObject(options, 'options');
  const { signal } = options;
  if (signal !== undefined) {
    validateAbortSignal(signal, 'options.signal');
  }
  start(options);
  const profilerOfThisCall = profiler;
  const { durationMillis = kDefaultDurationMillis } = options;
  try {
    const { setTimeout } = require('timers/promises');
    await setTimeout(durationMillis, undefined, { signal });
  } catch (err) {
    if (profiler === profilerOfThisCall) {
      stop(false);
    }
    throw err;
  }
  if (profiler !== profilerOfThisCall) {
    throw new ERR_INVALID_STATE(
      'The time profiler was stopped before the profile was complete');
  }
  return stop(false);
}

/**
 * Returns whether the profiler of this thread is started.
 * @returns {boolean} `true` if the profiler is started, `false` otherwise.
 */
function isStarted() {
  return profiler !== undefined;
}

/**
 * Returns whether the latest restart of the profiler found V8's CPU profiler
 * stuck.
 * @returns {number} 0 if no issue was detected, 1 if one possibly was, or 2
 *   if one certainly was. It is reset to 0 when the profiler is started, or
 *   stopped without being restarted.
 */
function v8ProfilerStuckEventLoopDetected() {
  return stuckEventLoopDetected;
}

/**
 * The names of the synthetic frames that the time profiler reports, exposed
 * as `pprof.time.constants`.
 * @type {{
 *   GARBAGE_COLLECTION_FUNCTION_NAME: string,
 *   NON_JS_THREADS_FUNCTION_NAME: string,
 * }}
 */
const constants = ObjectFreeze({
  GARBAGE_COLLECTION_FUNCTION_NAME,
  NON_JS_THREADS_FUNCTION_NAME,
});

module.exports = {
  constants,
  isStarted,
  profile,
  start,
  stop,
  v8ProfilerStuckEventLoopDetected,
};
