/* eslint-disable node-core/require-common-first, node-core/required-modules */

'use strict';

require('./');
const fs = require('fs');
const path = require('path');
const assert = require('assert');

function getCpuProfiles(dir) {
  const list = fs.readdirSync(dir);
  return list
    .filter((file) => file.endsWith('.cpuprofile'))
    .map((file) => path.join(dir, file));
}

function getFrames(file, suffix) {
  const data = fs.readFileSync(file, 'utf8');
  const profile = JSON.parse(data);
  const frames = profile.nodes.filter((i) => {
    const frame = i.callFrame;
    return frame.url.endsWith(suffix);
  });
  return { frames, nodes: profile.nodes };
}

function verifyFrames(output, file, suffix) {
  const { frames, nodes } = getFrames(file, suffix);
  if (frames.length === 0) {
    // Show native debug output and the profile for debugging.
    console.log(output.stderr.toString());
    console.log(nodes);
  }
  assert.notDeepStrictEqual(frames, []);
}

// We need to set --cpu-interval to a smaller value to make sure we can
// find our workload in the samples. 50us should be a small enough sampling
// interval for this.
const kCpuProfInterval = 50;
const env = {
  ...process.env,
  NODE_DEBUG_NATIVE: 'INSPECTOR_PROFILER'
};

module.exports = {
  getCpuProfiles,
  kCpuProfInterval,
  env,
  getFrames,
  verifyFrames
};
