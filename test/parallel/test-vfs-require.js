'use strict';

require('../common');
const assert = require('assert');
const fs = require('fs');

// Test requiring a simple virtual module
// VFS internal path: /hello.js
// Mount point: /virtual
// External path: /virtual/hello.js
{
  const myVfs = fs.createVirtual();
  myVfs.writeFileSync('/hello.js', 'module.exports = "hello from vfs";');
  myVfs.mount('/virtual');

  const result = require('/virtual/hello.js');
  assert.strictEqual(result, 'hello from vfs');

  myVfs.unmount();
}

// Test requiring a virtual module that exports an object
{
  const myVfs = fs.createVirtual();
  myVfs.writeFileSync('/config.js', `
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
  myVfs.writeFileSync('/utils.js', `
    module.exports = {
      add: function(a, b) { return a + b; }
    };
  `);
  myVfs.writeFileSync('/main.js', `
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
  myVfs.writeFileSync('/data.json', JSON.stringify({
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
  myVfs.mkdirSync('/my-package', { recursive: true });
  myVfs.writeFileSync('/my-package/package.json', JSON.stringify({
    name: 'my-package',
    main: 'index.js',
  }));
  myVfs.writeFileSync('/my-package/index.js', `
    module.exports = { loaded: true };
  `);
  myVfs.mount('/virtual5');

  const pkg = require('/virtual5/my-package');
  assert.strictEqual(pkg.loaded, true);

  myVfs.unmount();
}

// Test that real modules still work when VFS is mounted
{
  const myVfs = fs.createVirtual();
  myVfs.writeFileSync('/test.js', 'module.exports = 1;');
  myVfs.mount('/virtual6');

  // require('assert') should still work (builtin)
  assert.strictEqual(typeof assert.strictEqual, 'function');

  // Real file requires should still work
  const commonMod = require('../common');
  assert.ok(commonMod);

  myVfs.unmount();
}

// Test require with relative paths inside VFS module
{
  const myVfs = fs.createVirtual();
  myVfs.mkdirSync('/lib', { recursive: true });
  myVfs.writeFileSync('/lib/helper.js', `
    module.exports = { help: function() { return 'helped'; } };
  `);
  myVfs.writeFileSync('/lib/index.js', `
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
  myVfs.writeFileSync('/file.txt', 'virtual content');
  myVfs.mount('/virtual9');

  const content = fs.readFileSync('/virtual9/file.txt', 'utf8');
  assert.strictEqual(content, 'virtual content');

  myVfs.unmount();
}

// Test that unmounting stops interception
{
  const myVfs = fs.createVirtual();
  myVfs.writeFileSync('/unmount-test.js', 'module.exports = "before unmount";');
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
