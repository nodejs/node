// Flags: --expose-gc --expose-internals
'use strict';

const common = require('../common');
const { checkIfCollectableByCounting } = require('../common/gc');
const assert = require('assert');
const fs = require('node:fs');
const path = require('node:path');
const tmpdir = require('../common/tmpdir');
const { internalBinding } = require('internal/test/binding');
const { FSReqCallback } = internalBinding('fs');

// The CVE primarily affects Windows but we should test on all platforms

{
  tmpdir.refresh();
}

{
  const longFileNamePart = 'a'.repeat(200);
  const fileName = tmpdir.resolve(`long-file-name-${longFileNamePart}-for-memory-leak-test.txt`);
  fs.writeFileSync(fileName, 'test content', 'utf8');
  const fullPath = path.resolve(fileName);

  assert(fs.existsSync(fullPath), 'Test file should exist');

  async function runTest() {
    try {
      await checkIfCollectableByCounting(
        () => {
          for (let i = 0; i < 10; i++) {
            fs.existsSync(fullPath);
          }
          return 10;
        },
        FSReqCallback,
        10
      );
    } catch (err) {
      assert.ifError(err, 'Memory leak detected: FSReqCallback objects were not collected');
    } finally {
      tmpdir.refresh();
    }
  }

  runTest().then(common.mustCall());
}
