'use strict';

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
