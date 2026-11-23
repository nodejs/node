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
// Modified for inclusion in Node.js core: ported from the time profile parts
// of
// https://github.com/DataDog/pprof-nodejs/blob/3905f382723fc2554284e1abbba2df0f2dc6f82b/ts/src/profile-serializer.ts
// without labels, column numbers and source maps.
//
// SPDX-License-Identifier: Apache-2.0

'use strict';

const {
  ArrayPrototypeMap,
  ArrayPrototypePush,
  Number,
  SafeMap,
} = primordials;

const {
  Line,
  Location,
  Profile,
  ProfileFunction,
  Sample,
  StringTable,
  ValueType,
} = require('internal/perf/pprof/profile');
const {
  frameFunctionName,
  visitTimeProfile,
} = require('internal/perf/pprof/normalizer');

/**
 * Builds a pprof `Profile` message. The normalizer does not build the profile
 * itself: it walks a profile and calls methods of an object, the visitor, for
 * each frame, stack and sample it finds (see `TimeProfileVisitor` in
 * `internal/perf/pprof/normalizer`). This function creates a visitor whose
 * methods turn each frame into a `Location` and a `ProfileFunction`, and
 * each sample into a `Sample`, passes it to `visit`, and returns the
 * `Profile` made of what the visitor received.
 * @param {(visitor: TimeProfileVisitor) => void} visit Called once with the
 *   visitor, and must deliver a normalized profile to it, such as by calling
 *   `visitTimeProfile()` with it.
 * @returns {Profile} The profile, ready to be encoded.
 */
function serialize(visit) {
  const stringTable = new StringTable();
  const samples = [];
  const locations = [];
  const stacks = [];
  const functions = [];
  const functionIds = new SafeMap();
  let data = { __proto__: null };

  /**
   * Creates a pprof ValueType, interning its type and unit.
   * @param {{ type: string, unit: string }} type The names of the type and
   *   of the unit, such as `wall` and `nanoseconds`.
   * @returns {ValueType} The ValueType, which refers to the names by index
   *   in the string table.
   */
  function valueType(type) {
    return new ValueType({
      type: stringTable.dedup(type.type),
      unit: stringTable.dedup(type.unit),
    });
  }

  /**
   * Returns the pprof Function of a frame, creating it the first time.
   * @param {object} frame A frame delivered by the normalizer.
   * @returns {ProfileFunction} The Function, which the profile holds.
   */
  function getFunction(frame) {
    // A function is identified by script, file and name, or by script, file
    // and position for anonymous functions. The prefixes keep the two kinds
    // apart, and the file's length makes the key unambiguous.
    const file = frame.file;
    const key = frame.name ?
      `n:${frame.scriptId}:${file.length}:${file}:${frame.name}` :
      `a:${frame.scriptId}:${frame.line}:${frame.column}:${file}`;
    let id = functionIds.get(key);
    if (id !== undefined) {
      return functions[id - 1];
    }
    // IDs are indices + 1, since 0 is not a valid ID.
    id = functions.length + 1;
    functionIds.set(key, id);
    const nameId = stringTable.dedup(frameFunctionName(frame));
    const fn = new ProfileFunction({
      id,
      name: nameId,
      systemName: nameId,
      filename: stringTable.dedup(file),
    });
    ArrayPrototypePush(functions, fn);
    return fn;
  }

  visit({
    /**
     * Records the profile's sample types, times and period.
     * @param {object} metadata The metadata delivered by the normalizer: the
     *   `sampleTypes`, `startTimeNanos`, `durationNanos`, `periodType` and
     *   `period` of the profile.
     */
    metadata(metadata) {
      data = {
        __proto__: null,
        sampleType: ArrayPrototypeMap(metadata.sampleTypes, valueType),
        // pprof Profile objects have always had a number here, which rounds
        // the time to a double.
        timeNanos: Number(metadata.startTimeNanos),
        durationNanos: metadata.durationNanos,
        periodType: valueType(metadata.periodType),
        period: metadata.period,
      };
    },
    /**
     * Creates the pprof Location of a frame.
     * @param {object} frame A frame delivered by the normalizer, with its
     *   `name`, `file`, `line`, `column` and `scriptId`.
     * @returns {number} The ID of the Location.
     */
    frame(frame) {
      // IDs are indices + 1, since 0 is not a valid ID.
      const id = locations.length + 1;
      const line = new Line({
        functionId: getFunction(frame).id,
        line: frame.line,
      });
      ArrayPrototypePush(locations, new Location({ id, line: [line] }));
      return id;
    },
    /**
     * Records a stack.
     * @param {number[]} frameIds The Location IDs of the stack, leaf first.
     * @returns {number} The index of the stack.
     */
    stack(frameIds) {
      return ArrayPrototypePush(stacks, frameIds) - 1;
    },
    /**
     * Creates a pprof Sample.
     * @param {{ stackId: number, values: number[] }} sample A sample delivered
     *   by the normalizer: the index of its stack, as returned by `stack()`,
     *   and one value per sample type.
     */
    sample(sample) {
      ArrayPrototypePush(samples, new Sample({
        locationId: stacks[sample.stackId],
        value: sample.values,
      }));
    },
  });

  data.sample = samples;
  data.location = locations;
  data.function = functions;
  data.stringTable = stringTable;
  return new Profile(data);
}

/**
 * Converts a time profile produced by the pprof binding into a pprof Profile.
 * @param {object} profile The flat node table that the pprof binding's `stop()`
 *   returns.
 * @param {number} intervalMicros The requested average time between samples.
 * @param {boolean} [recomputeSamplingInterval] Whether to replace the
 *   requested interval with the average interval between samples.
 * @returns {Profile} The profile, ready to be encoded.
 */
function serializeTimeProfile(profile, intervalMicros,
                              recomputeSamplingInterval = false) {
  const options = { intervalMicros, recomputeSamplingInterval };
  return serialize((visitor) => visitTimeProfile(profile, visitor, options));
}

module.exports = {
  serializeTimeProfile,
};
