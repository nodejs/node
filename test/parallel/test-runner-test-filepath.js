'use strict';
const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
const { writeFileSync } = require('node:fs');
const { suite, test } = require('node:test');

tmpdir.refresh();

suite('suite', common.mustCall((t) => {
  assert.strictEqual(t.filePath, __filename);

  test('test', (t) => {
    assert.strictEqual(t.filePath, __filename);

    t.test('subtest', (t) => {
      assert.strictEqual(t.filePath, __filename);

      t.test('subsubtest', (t) => {
        assert.strictEqual(t.filePath, __filename);
      });
    });
  });
}));

test((t) => {
  assert.strictEqual(t.filePath, __filename);
});

const importedTestFile = tmpdir.resolve('temp.js');
writeFileSync(importedTestFile, `
  'use strict';
  const { strictEqual } = require('node:assert');
  const { suite, test } = require('node:test');

  suite('imported suite', (t) => {
    strictEqual(t.filePath, ${JSON.stringify(__filename)});

    test('imported test', (t) => {
      strictEqual(t.filePath, ${JSON.stringify(__filename)});

      t.test('imported subtest', (t) => {
        strictEqual(t.filePath, ${JSON.stringify(__filename)});

        t.test('imported subsubtest', (t) => {
          strictEqual(t.filePath, ${JSON.stringify(__filename)});
        });
      });
    });
  });
`);
require(importedTestFile);
