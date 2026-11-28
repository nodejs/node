// Copyright 2026 Datadog. All Rights Reserved.
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
// https://github.com/DataDog/pprof-nodejs/blob/3905f382723fc2554284e1abbba2df0f2dc6f82b/ts/src/profile-normalizer.ts
// without source maps and labels, and walking the flat node tables that the
// pprof binding produces rather than trees of nodes.
//
// SPDX-License-Identifier: Apache-2.0

'use strict';

// Format-neutral normalization of time profiles: turns the profiles that the
// pprof binding produces into a stream of frames, stacks and samples, applying
// everything that is a matter of profile semantics rather than of the output
// format: synthetic runtime frames, CPU time attribution and sample values.
// Output formats are implemented as visitors on top of it, as described by
// the TimeProfileVisitor type.

/**
 * Receives a normalized time profile. `metadata()` is called once, then
 * `frame()`, `stack()` and `sample()` are called interleaved. Each distinct
 * frame and stack is delivered once, before the first stack or sample that
 * refers to it, and only stacks that have samples are delivered.
 * @typedef {object} TimeProfileVisitor
 * @property {(metadata: object) => void} metadata Receives the
 *   `sampleTypes`, `startTimeNanos`, `durationNanos`, `periodType` and
 *   `period` of the profile.
 * @property {(frame: object) => any} frame Receives a frame, as
 *   `{ name, file, line, column, scriptId }`, and returns the ID by which
 *   stacks refer to it.
 * @property {(frameIds: any[]) => any} stack Receives the frame IDs of a
 *   stack, leaf first, and returns the ID by which samples refer to it.
 * @property {(sample: { stackId: any, values: number[] }) => void} sample
 *   Receives a sample: the ID of its stack, and one value per sample type.
 */

const {
  ArrayPrototypePush,
  BigInt,
  DateNow,
  Int32Array,
  MathFloor,
  MathMax,
  MathMin,
  MathRound,
  SafeMap,
  StringPrototypeSlice,
  StringPrototypeStartsWith,
} = primordials;

/**
 * The name of the synthetic frame that holds the CPU time of the threads that
 * do not run JavaScript, such as the threads of the libuv thread pool and of
 * V8. Exposed as `pprof.time.constants.NON_JS_THREADS_FUNCTION_NAME`.
 * @type {string}
 */
const NON_JS_THREADS_FUNCTION_NAME = 'Non JS threads activity';
/**
 * The name of the synthetic frame that the samples taken during garbage
 * collection are reported under, instead of V8's `(garbage collector)`.
 * Exposed as `pprof.time.constants.GARBAGE_COLLECTION_FUNCTION_NAME`.
 * @type {string}
 */
const GARBAGE_COLLECTION_FUNCTION_NAME = 'Garbage Collection';
/**
 * The name of the synthetic top level frame that the
 * `GARBAGE_COLLECTION_FUNCTION_NAME` and `NON_JS_THREADS_FUNCTION_NAME` frames
 * are under.
 * @type {string}
 */
const RUNTIME_FUNCTION_NAME = 'Node.js';

/**
 * Returns the name of the function that generated the frame, or, for an
 * anonymous function, a name made from its position, so that anonymous
 * functions can be told apart.
 * @param {{ name: string, line: number, column: number }} frame The frame,
 *   with the name of its function, which is empty for anonymous functions,
 *   and its position.
 * @returns {string} The name of the function, or
 *   `(anonymous:L#<line>:C#<column>)`, `(anonymous:L#<line>)` or
 *   `(anonymous)`, depending on which parts of the position are known.
 */
function frameFunctionName(frame) {
  if (frame.name) {
    return frame.name;
  }
  if (frame.line) {
    if (frame.column) {
      return `(anonymous:L#${frame.line}:C#${frame.column})`;
    }
    return `(anonymous:L#${frame.line})`;
  }
  return '(anonymous)';
}

/**
 * Delivers a time profile to a visitor.
 *
 * The profile's nodes are modified as follows:
 * - `(program)` nodes are removed.
 * - `(idle)` nodes are only kept when CPU time is attributed to them. Their
 *   samples keep their sample count, but get no wall time.
 * - `(garbage collector)` nodes are renamed `Garbage Collection`, and moved
 *   under a top level `Node.js` node.
 * - When CPU time was collected, a `Non JS threads activity` node, holding the
 *   CPU time of the threads that do not run JavaScript, is added under the
 *   `Node.js` node.
 *
 * Sample values are [sample count, wall nanoseconds], plus CPU nanoseconds if
 * the profile has CPU time.
 * @param {object} profile The flat node table that the pprof binding's
 *   `stop()` returns.
 * @param {TimeProfileVisitor} visitor The visitor that receives the
 *   normalized profile.
 * @param {object} options The sampling interval to report, and whether to
 *   recompute it from the profile.
 * @param {number} options.intervalMicros The requested sampling interval.
 * @param {boolean} options.recomputeSamplingInterval Whether to replace the
 *   requested interval with the average interval between samples.
 */
function visitTimeProfile(profile, visitor, options) {
  const {
    parent,
    name,
    scriptName,
    scriptId,
    lineNumber,
    columnNumber,
    hitCount,
    cpuTime,
    hasCpuTime,
  } = profile;
  const nodeCount = parent.length;

  let intervalMicros = options.intervalMicros;
  if (options.recomputeSamplingInterval && profile.totalHitCount > 0) {
    // The profile duration should be the number of hits times the interval.
    // In practice, intervals between samples are larger than requested (e.g.
    // 12.5ms vs 10ms), so an average is more accurate. For very short
    // profiles the average is meaningless (e.g. with a single hit), so it is
    // bounded to [intervalMicros, 2 * intervalMicros].
    intervalMicros = MathMin(
      MathMax(
        MathFloor((profile.endTime - profile.startTime) / profile.totalHitCount),
        intervalMicros,
      ),
      2 * intervalMicros,
    );
  }
  const intervalNanos = intervalMicros * 1000;

  const timeType = { type: 'wall', unit: 'nanoseconds' };
  const sampleTypes = [
    { type: 'sample', unit: 'count' },
    timeType,
  ];
  if (hasCpuTime) {
    ArrayPrototypePush(sampleTypes, { type: 'cpu', unit: 'nanoseconds' });
  }
  const durationNanos = (profile.endTime - profile.startTime) * 1000;
  // Exact with the binding's epoch offset. Without one, estimate it on the
  // assumption that the profile was just stopped.
  const startTimeNanos = profile.epochOffset !== undefined ?
    BigInt(MathRound(profile.startTime + profile.epochOffset)) * 1000n :
    BigInt(DateNow()) * 1000000n - BigInt(MathRound(durationNanos));
  visitor.metadata({
    sampleTypes,
    startTimeNanos,
    durationNanos,
    periodType: timeType,
    period: intervalNanos,
  });

  // The visitor's ID of each frame that was delivered, keyed by script ID,
  // position and name, as made by getFrameId(). V8 creates a node for each
  // call path, so a function called from several callers has several nodes:
  // the map delivers their frame to the visitor once, and gives them the same
  // frame ID, so that the visitor can share one location between them.
  const frameIds = new SafeMap();

  // Interned stacks, by internal stack index: the parent stack index (-1 for
  // none), the visitor's frame ID, the visitor's stack ID (undefined until
  // delivered), and the child stacks by frame ID. Stacks are interned by
  // prefix, so stacks with equal frames get the same ID without comparing
  // whole arrays.
  const stackParents = [];
  const stackFrames = [];
  const stackIds = [];
  const stackChildren = [];
  const rootStacks = new SafeMap();

  /**
   * Returns the index of the interned stack made of `frameId` on top of
   * `parentStack`, interning it if needed.
   * @param {number} parentStack The index of the interned stack of the
   *   caller, or -1 for a stack of one frame.
   * @param {any} frameId The ID that the visitor's `frame()` returned for the
   *   top frame.
   * @returns {number} The index of the interned stack.
   */
  function internStack(parentStack, frameId) {
    let children;
    if (parentStack < 0) {
      children = rootStacks;
    } else {
      children = stackChildren[parentStack];
      if (children === undefined) {
        children = new SafeMap();
        stackChildren[parentStack] = children;
      }
    }
    let stack = children.get(frameId);
    if (stack === undefined) {
      stack = ArrayPrototypePush(stackParents, parentStack) - 1;
      ArrayPrototypePush(stackFrames, frameId);
      ArrayPrototypePush(stackIds, undefined);
      ArrayPrototypePush(stackChildren, undefined);
      children.set(frameId, stack);
    }
    return stack;
  }

  /**
   * Returns the visitor's ID of an interned stack, delivering the stack to
   * the visitor the first time.
   * @param {number} stack The index of the interned stack.
   * @returns {any} The ID that the visitor's `stack()` returned for the stack.
   */
  function getStackId(stack) {
    let id = stackIds[stack];
    if (id === undefined) {
      const stackFrameIds = [];
      for (let s = stack; s !== -1; s = stackParents[s]) {
        ArrayPrototypePush(stackFrameIds, stackFrames[s]);
      }
      id = visitor.stack(stackFrameIds);
      stackIds[stack] = id;
    }
    return id;
  }

  /**
   * Returns the visitor's ID of a frame, delivering the frame to the visitor
   * the first time.
   * @param {string} frameName The name of the function of the frame, which
   *   is empty for anonymous functions.
   * @param {string} file The path or URL of the script of the frame, or an
   *   empty string for frames without one.
   * @param {number} frameScriptId The V8 ID of the script of the frame, or 0
   *   for frames without one.
   * @param {number} line The line of the frame: the first line of the
   *   function, or, with line numbers, the line that was executing.
   * @param {number} column The column of the frame, or 0 if unknown.
   * @returns {any} The ID that the visitor's `frame()` returned for the
   *   frame. Its type is up to the visitor; the serializer returns numbers.
   */
  function getFrameId(frameName, file, frameScriptId, line, column) {
    // The name comes last, so that the key is unambiguous.
    const key = `${frameScriptId}:${line}:${column}:${frameName}`;
    let id = frameIds.get(key);
    if (id === undefined) {
      id = visitor.frame({
        name: frameName,
        file,
        line,
        column,
        scriptId: frameScriptId,
      });
      frameIds.set(key, id);
    }
    return id;
  }

  let runtimeStack;
  /**
   * Returns the index of the stack of the synthetic top level `Node.js`
   * frame, interning it the first time.
   * @returns {number} The index of the interned stack.
   */
  function getRuntimeStack() {
    runtimeStack ??= internStack(-1,
                                 getFrameId(RUNTIME_FUNCTION_NAME, '', 0, 0, 0));
    return runtimeStack;
  }

  /**
   * Delivers a sample of an interned stack to the visitor.
   * @param {number} stack The index of the interned stack.
   * @param {number[]} values One value per sample type.
   */
  function emitSample(stack, values) {
    visitor.sample({ stackId: getStackId(stack), values });
  }

  // The stack of each node, or -1 for nodes that are removed along with their
  // descendants. Node 0 is the root, which has no frame.
  const nodeStacks = new Int32Array(nodeCount);
  nodeStacks[0] = -1;
  for (let i = 1; i < nodeCount; i++) {
    let nodeName = name[i];
    let parentStack;
    if (parent[i] === 0) {
      if (nodeName === '(program)') {
        nodeStacks[i] = -1;
        continue;
      }
      if (nodeName === '(garbage collector)') {
        nodeName = GARBAGE_COLLECTION_FUNCTION_NAME;
        parentStack = getRuntimeStack();
      } else {
        parentStack = -1;
      }
    } else {
      parentStack = nodeStacks[parent[i]];
      if (parentStack === -1) {
        nodeStacks[i] = -1;
        continue;
      }
    }

    // .mjs files have a `file://` prefix in the script name.
    let file = scriptName[i];
    if (StringPrototypeStartsWith(file, 'file://')) {
      file = StringPrototypeSlice(file, 7);
    }
    const stack = internStack(
      parentStack,
      getFrameId(nodeName, file, scriptId[i], lineNumber[i], columnNumber[i]),
    );
    nodeStacks[i] = stack;

    const hits = hitCount[i];
    const cpu = hasCpuTime ? cpuTime[i] : 0;
    // `(idle)` nodes only matter for the CPU time attributed to them, if any:
    // their samples keep their sample count, but get no wall time.
    const isIdle = nodeName === '(idle)';
    if ((!isIdle && hits > 0) || cpu > 0) {
      const values = hits > 0 ?
        [hits, isIdle ? 0 : hits * intervalNanos] :
        [0, 0];
      if (hasCpuTime) {
        ArrayPrototypePush(values, cpu);
      }
      emitSample(stack, values);
    }
  }

  if (hasCpuTime && profile.nonJSThreadsCpuTime) {
    // No hits, since this is not wall time of the profiled thread.
    const stack = internStack(
      getRuntimeStack(),
      getFrameId(NON_JS_THREADS_FUNCTION_NAME, '', 0, 0, 0),
    );
    emitSample(stack, [
      0,
      0,
      profile.nonJSThreadsCpuTime,
    ]);
  }
}

module.exports = {
  GARBAGE_COLLECTION_FUNCTION_NAME,
  NON_JS_THREADS_FUNCTION_NAME,
  frameFunctionName,
  visitTimeProfile,
};
