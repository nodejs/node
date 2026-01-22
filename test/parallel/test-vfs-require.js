'use strict';

require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');

// Test requiring a simple virtual module
// VFS internal path: /hello.js
// Mount point: /virtual
// External path: /virtual/hello.js
{
  const myVfs = fs.createVirtual();
  myVfs.addFile('/hello.js', 'module.exports = "hello from vfs";');
  myVfs.mount('/virtual');

  const result = require('/virtual/hello.js');
  assert.strictEqual(result, 'hello from vfs');

  myVfs.unmount();
}

// Test requiring a virtual module that exports an object
{
  const myVfs = fs.createVirtual();
  myVfs.addFile('/config.js', `
    module.exports = {
      name: 'test-config',
      version: '1.0.0',
      getValue: function() { return 42; }
    };
  `);
  myVfs.mount('/virtual2');

  const config = require('/virtual2/config.js');
  assert.strictEqual(config.name, 'test-config');
  assert.strictEqual(config.version, '1.0.0');
  assert.strictEqual(config.getValue(), 42);

  myVfs.unmount();
}

// Test requiring a virtual module that requires another virtual module
{
  const myVfs = fs.createVirtual();
  myVfs.addFile('/utils.js', `
    module.exports = {
      add: function(a, b) { return a + b; }
    };
  `);
  myVfs.addFile('/main.js', `
    const utils = require('/virtual3/utils.js');
    module.exports = {
      sum: utils.add(10, 20)
    };
  `);
  myVfs.mount('/virtual3');

  const main = require('/virtual3/main.js');
  assert.strictEqual(main.sum, 30);

  myVfs.unmount();
}

// Test requiring a JSON file from VFS
{
  const myVfs = fs.createVirtual();
  myVfs.addFile('/data.json', JSON.stringify({
    items: [1, 2, 3],
    enabled: true,
  }));
  myVfs.mount('/virtual4');

  const data = require('/virtual4/data.json');
  assert.deepStrictEqual(data.items, [1, 2, 3]);
  assert.strictEqual(data.enabled, true);

  myVfs.unmount();
}

// Test virtual package.json resolution
{
  const myVfs = fs.createVirtual();
  myVfs.addFile('/my-package/package.json', JSON.stringify({
    name: 'my-package',
    main: 'index.js',
  }));
  myVfs.addFile('/my-package/index.js', `
    module.exports = { loaded: true };
  `);
  myVfs.mount('/virtual5');

  const pkg = require('/virtual5/my-package');
  assert.strictEqual(pkg.loaded, true);

  myVfs.unmount();
}

// Test overlay mode with require
{
  const myVfs = fs.createVirtual();
  const testPath = path.join(__dirname, '../fixtures/vfs-test/overlay-module.js');

  // Create a virtual module at a path that doesn't exist on disk
  // In overlay mode, we use the full path as the VFS internal path
  myVfs.addFile(testPath, 'module.exports = "overlay module";');
  myVfs.overlay();

  const result = require(testPath);
  assert.strictEqual(result, 'overlay module');

  myVfs.unmount();

  // Clear from cache so next tests work
  delete require.cache[testPath];
}

// Test that real modules still work when VFS is mounted
{
  const myVfs = fs.createVirtual();
  myVfs.addFile('/test.js', 'module.exports = 1;');
  myVfs.mount('/virtual6');

  // require('assert') should still work (builtin)
  assert.strictEqual(typeof assert.strictEqual, 'function');

  // Real file requires should still work
  const commonMod = require('../common');
  assert.ok(commonMod);

  myVfs.unmount();
}

// Test dynamic content for modules
{
  const myVfs = fs.createVirtual();
  let counter = 0;

  myVfs.addFile('/dynamic.js', () => {
    counter++;
    return `module.exports = ${counter};`;
  });
  myVfs.mount('/virtual7');

  // First require - counter becomes 1
  let result = require('/virtual7/dynamic.js');
  assert.strictEqual(result, 1);

  // Clear cache and require again - counter becomes 2
  delete require.cache['/virtual7/dynamic.js'];
  result = require('/virtual7/dynamic.js');
  assert.strictEqual(result, 2);

  myVfs.unmount();
}

// Test require with relative paths inside VFS module
{
  const myVfs = fs.createVirtual();
  myVfs.addFile('/lib/helper.js', `
    module.exports = { help: function() { return 'helped'; } };
  `);
  myVfs.addFile('/lib/index.js', `
    const helper = require('./helper.js');
    module.exports = helper.help();
  `);
  myVfs.mount('/virtual8');

  const result = require('/virtual8/lib/index.js');
  assert.strictEqual(result, 'helped');

  myVfs.unmount();
}

// Test fs.readFileSync interception when VFS is active
{
  const myVfs = fs.createVirtual();
  myVfs.addFile('/file.txt', 'virtual content');
  myVfs.mount('/virtual9');

  const content = fs.readFileSync('/virtual9/file.txt', 'utf8');
  assert.strictEqual(content, 'virtual content');

  myVfs.unmount();
}

// Test that unmounting stops interception
{
  const myVfs = fs.createVirtual();
  myVfs.addFile('/unmount-test.js', 'module.exports = "before unmount";');
  myVfs.mount('/virtual10');

  const result = require('/virtual10/unmount-test.js');
  assert.strictEqual(result, 'before unmount');

  myVfs.unmount();

  // After unmounting, the file should not be found
  assert.throws(() => {
    // Clear require cache first
    delete require.cache['/virtual10/unmount-test.js'];
    require('/virtual10/unmount-test.js');
  }, { code: 'MODULE_NOT_FOUND' });
}
