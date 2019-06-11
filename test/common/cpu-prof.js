/* eslint-disable node-core/require-common-first, node-core/required-modules */

'use strict';

const common = require('./');
const fs = require('fs');
const path = require('path');
const assert = require('assert');

function getCpuProfiles(dir) {
  const list = fs.readdirSync(dir);
  return list
    .filter((file) => file.endsWith('.cpuprofile'))
    .map((file) => path.join(dir, file));
}

function getFrames(output, file, suffix) {
  const data = fs.readFileSync(file, 'utf8');
  const profile = JSON.parse(data);
  const frames = profile.nodes.filter((i) => {
    const frame = i.callFrame;
    return frame.url.endsWith(suffix);
  });
  return { frames, nodes: profile.nodes };
}

function verifyFrames(output, file, suffix) {
  const { frames, nodes } = getFrames(output, file, suffix);
  if (frames.length === 0) {
    // Show native debug output and the profile for debugging.
    console.log(output.stderr.toString());
    console.log(nodes);
  }
  assert.notDeepStrictEqual(frames, []);
}

let FIB = 30;
// This is based on emperial values - in the CI, on Windows the program
// tend to finish too fast then we won't be able to see the profiled script
// in the samples, so we need to bump the values a bit. On slower platforms
// like the Pis it could take more time to complete, we need to use a
// smaller value so the test would not time out.
if (common.isWindows) {
  FIB = 40;
}

// We need to set --cpu-interval to a smaller value to make sure we can
// find our workload in the samples. 50us should be a small enough sampling
// interval for this.
const kCpuProfInterval = 50;
const env = {
  ...process.env,
  FIB,
  NODE_DEBUG_NATIVE: 'INSPECTOR_PROFILER'
};

module.exports = {
  getCpuProfiles,
  kCpuProfInterval,
  env,
  getFrames,
  verifyFrames
};
