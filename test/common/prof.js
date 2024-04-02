'use strict';

const assert = require('assert');
const fs = require('fs');
const path = require('path');

function getHeapProfiles(dir) {
  const list = fs.readdirSync(dir);
  return list
    .filter((file) => file.endsWith('.heapprofile'))
    .map((file) => path.join(dir, file));
}

function findFirstFrameInNode(root, func) {
  const first = root.children.find(
    (child) => child.callFrame.functionName === func,
  );
  if (first) {
    return first;
  }
  for (const child of root.children) {
    const first = findFirstFrameInNode(child, func);
    if (first) {
      return first;
    }
  }
  return undefined;
}

function findFirstFrame(file, func) {
  const data = fs.readFileSync(file, 'utf8');
  const profile = JSON.parse(data);
  const first = findFirstFrameInNode(profile.head, func);
  return { frame: first, roots: profile.head.children };
}

function verifyFrames(output, file, func) {
  const { frame, roots } = findFirstFrame(file, func);
  if (!frame) {
    // Show native debug output and the profile for debugging.
    console.log(output.stderr.toString());
    console.log(roots);
  }
  assert.notStrictEqual(frame, undefined);
}

// We need to set --heap-prof-interval to a small enough value to make
// sure we can find our workload in the samples, so we need to set
// TEST_ALLOCATION > kHeapProfInterval.
const kHeapProfInterval = 128;
const TEST_ALLOCATION = kHeapProfInterval * 2;

const env = {
  ...process.env,
  TEST_ALLOCATION,
  NODE_DEBUG_NATIVE: 'INSPECTOR_PROFILER',
};

// TODO(joyeecheung): share the fixutres with v8 coverage tests
module.exports = {
  getHeapProfiles,
  verifyFrames,
  findFirstFrame,
  kHeapProfInterval,
  TEST_ALLOCATION,
  env,
};
