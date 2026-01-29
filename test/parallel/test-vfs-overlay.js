'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

tmpdir.refresh();

const testDir = path.join(tmpdir.path, 'vfs-overlay');
fs.mkdirSync(testDir, { recursive: true });

// Create real files for testing
fs.writeFileSync(path.join(testDir, 'real.txt'), 'real content');
fs.writeFileSync(path.join(testDir, 'both.txt'), 'real both');
fs.mkdirSync(path.join(testDir, 'subdir'));
fs.writeFileSync(path.join(testDir, 'subdir', 'nested.txt'), 'nested real');

// Test overlay mode property
{
  const normalVfs = vfs.create();
  assert.strictEqual(normalVfs.overlay, false);

  const overlayVfs = vfs.create({ overlay: true });
  assert.strictEqual(overlayVfs.overlay, true);
}

// Test overlay mode: only intercepts files that exist in VFS
{
  const overlayVfs = vfs.create(new vfs.MemoryProvider(), { overlay: true });

  // Add a file to VFS that also exists on real fs
  overlayVfs.writeFileSync('/both.txt', 'virtual both');

  // Add a file that only exists in VFS
  overlayVfs.writeFileSync('/virtual-only.txt', 'only in vfs');

  // Mount at the test directory path
  overlayVfs.mount(testDir);

  // File exists in VFS - should be intercepted
  assert.strictEqual(overlayVfs.shouldHandle(path.join(testDir, 'both.txt')), true);
  assert.strictEqual(overlayVfs.shouldHandle(path.join(testDir, 'virtual-only.txt')), true);

  // File only exists on real fs - should NOT be intercepted in overlay mode
  assert.strictEqual(overlayVfs.shouldHandle(path.join(testDir, 'real.txt')), false);

  // Non-existent file - should NOT be intercepted
  assert.strictEqual(overlayVfs.shouldHandle(path.join(testDir, 'nonexistent.txt')), false);

  overlayVfs.unmount();
}

// Test overlay mode with fs module integration
{
  const overlayVfs = vfs.create(new vfs.MemoryProvider(), { overlay: true });

  // Mock a specific file
  overlayVfs.writeFileSync('/both.txt', 'mocked content');

  overlayVfs.mount(testDir);

  // Mocked file returns VFS content
  const mockedContent = fs.readFileSync(path.join(testDir, 'both.txt'), 'utf8');
  assert.strictEqual(mockedContent, 'mocked content');

  // Real file (not in VFS) returns real content
  const realContent = fs.readFileSync(path.join(testDir, 'real.txt'), 'utf8');
  assert.strictEqual(realContent, 'real content');

  // Nested real file also works
  const nestedContent = fs.readFileSync(path.join(testDir, 'subdir', 'nested.txt'), 'utf8');
  assert.strictEqual(nestedContent, 'nested real');

  overlayVfs.unmount();
}

// Test non-overlay mode (default): intercepts all paths under mount point
{
  const normalVfs = vfs.create(new vfs.MemoryProvider());

  // Add one file
  normalVfs.writeFileSync('/some-file.txt', 'vfs content');

  normalVfs.mount(testDir);

  // All paths under mount point should be handled, even if they don't exist in VFS
  assert.strictEqual(normalVfs.shouldHandle(path.join(testDir, 'some-file.txt')), true);
  assert.strictEqual(normalVfs.shouldHandle(path.join(testDir, 'real.txt')), true);
  assert.strictEqual(normalVfs.shouldHandle(path.join(testDir, 'nonexistent.txt')), true);

  // Paths outside mount point should not be handled
  assert.strictEqual(normalVfs.shouldHandle('/other/path'), false);

  normalVfs.unmount();
}

// Test overlay mode with directories
{
  const overlayVfs = vfs.create(new vfs.MemoryProvider(), { overlay: true });

  // Create a directory in VFS
  overlayVfs.mkdirSync('/vfs-dir');
  overlayVfs.writeFileSync('/vfs-dir/file.txt', 'vfs file');

  overlayVfs.mount(testDir);

  // VFS directory should be handled
  assert.strictEqual(overlayVfs.shouldHandle(path.join(testDir, 'vfs-dir')), true);
  assert.strictEqual(overlayVfs.shouldHandle(path.join(testDir, 'vfs-dir', 'file.txt')), true);

  // Real directory should NOT be handled in overlay mode
  assert.strictEqual(overlayVfs.shouldHandle(path.join(testDir, 'subdir')), false);

  overlayVfs.unmount();
}

// Test overlay mode with require/import (async)
(async () => {
  const overlayVfs = vfs.create(new vfs.MemoryProvider(), { overlay: true });

  // Create a module in VFS
  overlayVfs.writeFileSync('/mock-module.js', 'module.exports = "mocked";');

  overlayVfs.mount(testDir);

  // Require the mocked module
  const result = require(path.join(testDir, 'mock-module.js'));
  assert.strictEqual(result, 'mocked');

  overlayVfs.unmount();

  // Clean up module cache
  delete require.cache[path.join(testDir, 'mock-module.js')];
})().then(common.mustCall());

// Test overlay mode validates boolean option
{
  assert.throws(() => {
    vfs.create({ overlay: 'true' });
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });

  assert.throws(() => {
    vfs.create({ overlay: 1 });
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });
}

// Test overlay mode with virtualCwd and chdir
// In overlay mode, chdir to a directory that exists only on real fs falls through
{
  const overlayVfs = vfs.create(new vfs.MemoryProvider(), {
    overlay: true,
    virtualCwd: true,
  });

  // Create a directory only in VFS
  overlayVfs.mkdirSync('/vfs-only-dir');

  overlayVfs.mount(testDir);

  // Chdir to VFS directory works (exists in VFS)
  overlayVfs.chdir(path.join(testDir, 'vfs-only-dir'));
  assert.strictEqual(overlayVfs.cwd(), path.join(testDir, 'vfs-only-dir'));

  // In overlay mode, shouldHandle returns false for real-only directories
  // So chdir to real directory would fall through to process.chdir
  // We verify this by checking shouldHandle
  assert.strictEqual(overlayVfs.shouldHandle(path.join(testDir, 'subdir')), false);

  // The mountpoint itself should always be handled (root exists)
  assert.strictEqual(overlayVfs.shouldHandle(testDir), true);

  overlayVfs.unmount();
}

// Test overlay mode in worker thread scenario
// This verifies the behavior documented: overlay allows selective mocking
{
  const { Worker, isMainThread } = require('worker_threads');

  if (isMainThread) {
    // Main thread: create worker with overlay VFS test
    const workerCode = `
      const { workerData, parentPort } = require('worker_threads');
      const vfs = require('node:vfs');
      const fs = require('fs');
      const path = require('path');

      const testDir = workerData.testDir;

      // In worker: create overlay VFS that mocks specific files
      const overlayVfs = vfs.create(new vfs.MemoryProvider(), {
        overlay: true,
        virtualCwd: true,
      });

      // Mock a specific config file
      overlayVfs.mkdirSync('/config');
      overlayVfs.writeFileSync('/config/app.json', '{"env": "test"}');

      overlayVfs.mount(testDir);

      // Read mocked file
      const mockedConfig = fs.readFileSync(path.join(testDir, 'config', 'app.json'), 'utf8');

      // Read real file (not mocked) - should work in overlay mode
      const realFile = fs.readFileSync(path.join(testDir, 'real.txt'), 'utf8');

      // Test chdir to VFS directory
      overlayVfs.chdir(path.join(testDir, 'config'));
      const cwd = overlayVfs.cwd();

      overlayVfs.unmount();

      parentPort.postMessage({
        mockedConfig,
        realFile,
        cwd,
      });
    `;

    const worker = new Worker(workerCode, {
      eval: true,
      workerData: { testDir },
    });

    worker.on('message', common.mustCall((msg) => {
      assert.strictEqual(msg.mockedConfig, '{"env": "test"}');
      assert.strictEqual(msg.realFile, 'real content');
      assert.strictEqual(msg.cwd, path.join(testDir, 'config'));
    }));

    worker.on('error', common.mustNotCall());
  }
}
