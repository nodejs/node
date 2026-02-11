'use strict';

const common = require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

// Test vfs-mount event
{
  const mountEvents = [];
  const mountListener = common.mustCall((info) => {
    mountEvents.push(info);
  });

  process.on('vfs-mount', mountListener);

  const myVfs = vfs.create();
  myVfs.writeFileSync('/test.txt', 'hello');
  myVfs.mount('/virtual');

  assert.strictEqual(mountEvents.length, 1);
  assert.strictEqual(mountEvents[0].mountPoint, '/virtual');
  assert.strictEqual(mountEvents[0].overlay, false);
  assert.strictEqual(mountEvents[0].readonly, false);

  myVfs.unmount();
  process.removeListener('vfs-mount', mountListener);
}

// Test vfs-unmount event
{
  const unmountEvents = [];
  const unmountListener = common.mustCall((info) => {
    unmountEvents.push(info);
  });

  process.on('vfs-unmount', unmountListener);

  const myVfs = vfs.create();
  myVfs.writeFileSync('/test.txt', 'hello');
  myVfs.mount('/virtual');
  myVfs.unmount();

  assert.strictEqual(unmountEvents.length, 1);
  assert.strictEqual(unmountEvents[0].mountPoint, '/virtual');
  assert.strictEqual(unmountEvents[0].overlay, false);
  assert.strictEqual(unmountEvents[0].readonly, false);

  process.removeListener('vfs-unmount', unmountListener);
}

// Test events with overlay mode
{
  const mountEvents = [];
  const unmountEvents = [];

  const mountListener = common.mustCall((info) => {
    mountEvents.push(info);
  });
  const unmountListener = common.mustCall((info) => {
    unmountEvents.push(info);
  });

  process.on('vfs-mount', mountListener);
  process.on('vfs-unmount', unmountListener);

  const myVfs = vfs.create({ overlay: true });
  myVfs.writeFileSync('/test.txt', 'hello');
  myVfs.mount('/');

  assert.strictEqual(mountEvents.length, 1);
  assert.strictEqual(mountEvents[0].mountPoint, '/');
  assert.strictEqual(mountEvents[0].overlay, true);

  myVfs.unmount();

  assert.strictEqual(unmountEvents.length, 1);
  assert.strictEqual(unmountEvents[0].overlay, true);

  process.removeListener('vfs-mount', mountListener);
  process.removeListener('vfs-unmount', unmountListener);
}

// Test events with readonly mode
{
  const mountEvents = [];

  const mountListener = common.mustCall((info) => {
    mountEvents.push(info);
  });

  process.on('vfs-mount', mountListener);

  const myVfs = vfs.create();
  myVfs.writeFileSync('/test.txt', 'hello');
  myVfs.provider.setReadOnly();
  myVfs.mount('/virtual');

  assert.strictEqual(mountEvents.length, 1);
  assert.strictEqual(mountEvents[0].readonly, true);

  myVfs.unmount();
  process.removeListener('vfs-mount', mountListener);
}

// Test that unmount on non-mounted VFS does not emit event
{
  const unmountEvents = [];
  const unmountListener = (info) => {
    unmountEvents.push(info);
  };

  process.on('vfs-unmount', unmountListener);

  const myVfs = vfs.create();
  myVfs.unmount(); // Unmount without mounting first

  assert.strictEqual(unmountEvents.length, 0);

  process.removeListener('vfs-unmount', unmountListener);
}

// Test using declaration triggers unmount event
{
  const unmountEvents = [];
  const unmountListener = common.mustCall((info) => {
    unmountEvents.push(info);
  });

  process.on('vfs-unmount', unmountListener);

  {
    using myVfs = vfs.create();
    myVfs.writeFileSync('/test.txt', 'hello');
    myVfs.mount('/virtual');
  } // VFS is automatically unmounted here via Symbol.dispose

  assert.strictEqual(unmountEvents.length, 1);
  assert.strictEqual(unmountEvents[0].mountPoint, '/virtual');

  process.removeListener('vfs-unmount', unmountListener);
}
