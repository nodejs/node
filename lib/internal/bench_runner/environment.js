'use strict';

const {
  ArrayPrototypePush,
  ArrayPrototypeSlice,
} = primordials;

function groupCpus(cpus) {
  const groups = [];
  for (let i = 0; i < cpus.length; i++) {
    // The reported speed is the current core frequency, which fluctuates
    // with frequency scaling, so only the model is used for grouping.
    const { model } = cpus[i];
    let group;
    for (let j = 0; j < groups.length; j++) {
      if (groups[j].model === model) {
        group = groups[j];
        break;
      }
    }
    if (group === undefined) {
      group = { __proto__: null, model, count: 0 };
      ArrayPrototypePush(groups, group);
    }
    group.count++;
  }
  return groups;
}

function collectEnvironment() {
  const os = require('os');
  const total = os.totalmem();
  const constrained = process.constrainedMemory();
  return {
    __proto__: null,
    node: process.version,
    v8: process.versions.v8,
    platform: process.platform,
    arch: process.arch,
    os: {
      __proto__: null,
      type: os.type(),
      release: os.release(),
      machine: os.machine(),
    },
    cpus: groupCpus(os.cpus()),
    availableParallelism: os.availableParallelism(),
    memory: {
      __proto__: null,
      total,
      free: os.freemem(),
      constrained: constrained > 0 && constrained < total ? constrained : null,
    },
    loadavg: os.loadavg(),
    execArgv: ArrayPrototypeSlice(process.execArgv),
  };
}

module.exports = {
  collectEnvironment,
};
