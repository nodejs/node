// Flags: --experimental-vfs
'use strict';

// writeFile / appendFile accept explicit { flag, mode } options.

const common = require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

// writeFileSync / promises.writeFile with explicit flag and mode
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/a.txt', 'hello', { flag: 'w', mode: 0o600 });
  assert.strictEqual(myVfs.readFileSync('/a.txt', 'utf8'), 'hello');

  myVfs.promises.writeFile('/b.txt', 'world', { flag: 'w', mode: 0o600 })
    .then(common.mustCall(() => {
      assert.strictEqual(myVfs.readFileSync('/b.txt', 'utf8'), 'world');
    }));
}

// appendFileSync / promises.appendFile with explicit flag and mode
{
  const myVfs = vfs.create();
  myVfs.appendFileSync('/a.txt', 'first', { flag: 'a', mode: 0o600 });
  myVfs.appendFileSync('/a.txt', '-second', { flag: 'a' });
  assert.strictEqual(myVfs.readFileSync('/a.txt', 'utf8'), 'first-second');

  myVfs.promises.appendFile('/b.txt', 'go', { flag: 'a', mode: 0o600 })
    .then(common.mustCall(() => {
      assert.strictEqual(myVfs.readFileSync('/b.txt', 'utf8'), 'go');
    }));
}
