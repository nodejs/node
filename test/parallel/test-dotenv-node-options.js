// Flags: --env-file test/fixtures/dotenv/node-options.env
'use strict';

require('../common');
const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');

assert.strictEqual(process.env.CUSTOM_VARIABLE, 'hello-world');

{
  // Only read permissions are enabled.
  // The following tests if write permissions are enabled through NODE_OPTIONS
  const filePath = path.join(__dirname, 'should-not-write.txt');
  assert.throws(
    () => fs.writeFileSync(filePath, 'hello', 'utf-8'),
    {
      code: 'ERR_ACCESS_DENIED',
      permission: 'FileSystemWrite',
    },
  );
}

// Test TZ environment variable
assert(new Date().toString().includes('Irish Standard Time'));
