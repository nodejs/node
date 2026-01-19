// This script relies on resources/resource-loaders.js. Include it before in order for the below
// methods to work properly.

// The resources used to trigger new entries.
const scriptResources = [
    'resources/empty.js',
    'resources/empty_script.js',
    'resources/empty.js?id'
];

const waitForNextTask = () => {
    return new Promise(resolve => {
        step_timeout(resolve, 0);
    });
};

const clearBufferAndSetSize = size => {
  performance.clearResourceTimings();
  performance.setResourceTimingBufferSize(size);
}

const forceBufferFullEvent = async () => {
  clearBufferAndSetSize(1);
  return new Promise(async resolve => {
    performance.addEventListener('resourcetimingbufferfull', resolve);
    // Load 2 resources to ensure onresourcetimingbufferfull is fired.
    // Load them in order in order to get the entries in that order!
    await load.script(scriptResources[0]);
    await load.script(scriptResources[1]);
  });
};

const fillUpTheBufferWithSingleResource = async (src = scriptResources[0]) => {
  clearBufferAndSetSize(1);
  await load.script(src);
};

const fillUpTheBufferWithTwoResources = async () => {
    clearBufferAndSetSize(2);
    // Load them in order in order to get the entries in that order!
    await load.script(scriptResources[0]);
    await load.script(scriptResources[1]);
};

const addAssertUnreachedBufferFull = t => {
  performance.addEventListener('resourcetimingbufferfull', t.step_func(() => {
    assert_unreached("resourcetimingbufferfull should not fire")
  }));
};

const checkEntries = numEntries => {
  const entries = performance.getEntriesByType('resource');
  assert_equals(entries.length, numEntries,
      'Number of entries does not match the expected value.');
  assert_true(entries[0].name.includes(scriptResources[0]),
      scriptResources[0] + " is in the entries buffer");
  if (entries.length > 1) {
    assert_true(entries[1].name.includes(scriptResources[1]),
        scriptResources[1] + " is in the entries buffer");
  }
  if (entries.length > 2) {
    assert_true(entries[2].name.includes(scriptResources[2]),
        scriptResources[2] + " is in the entries buffer");
  }
}

const bufferFullFirePromise = new Promise(resolve => {
  performance.addEventListener('resourcetimingbufferfull', async () => {
    // Wait for the next task just to ensure that all bufferfull events have fired, and to ensure
    // that the secondary buffer is copied (as this is an event, there may be microtask checkpoints
    // right after running an event handler).
    await waitForNextTask();
    resolve();
  });
});
