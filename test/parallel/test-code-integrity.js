// Flags: --expose-internals

'use strict';

const common = require('../common');
const assert = require('node:assert');
const { describe, it } = require('node:test');

// This functionality is currently only on Windows
if (!common.isWindows) {
  common.skip('Windows specific test.');
}

const ci = require('internal/code_integrity');

describe('cjs loader code integrity integration tests', () => {
  it('should throw an error if a .js file does not pass code integrity policy',
     (t) => {
       t.mock.method(ci, ci.isAllowedToExecuteFile.name, () => { return false; });

       assert.throws(
         () => {
           require('../fixtures/code_integrity_test.js');
         },
         {
           code: 'ERR_CODE_INTEGRITY_VIOLATION',
         },
       );
     }
  );
  it('should NOT throw an error if a .js file passes code integrity policy',
     (t) => {
       t.mock.method(ci, ci.isAllowedToExecuteFile.name, () => { return true; });

       assert.ok(
         require('../fixtures/code_integrity_test.js')
       );
     }
  );
  it('should throw an error if a .json file does not pass code integrity policy',
     (t) => {
       t.mock.method(ci, ci.isAllowedToExecuteFile.name, () => { return false; });

       assert.throws(
         () => {
           require('../fixtures/code_integrity_test.json');
         },
         {
           code: 'ERR_CODE_INTEGRITY_VIOLATION',
         },
       );
     }
  );
  it('should NOT throw an error if a .json file passes code integrity policy',
     (t) => {
       t.mock.method(ci, ci.isAllowedToExecuteFile.name, () => { return true; });

       assert.ok(
         require('../fixtures/code_integrity_test.json')
       );
     }
  );
  it('should NOT check integrity for builtin modules',
     (t) => {
       // Mock isAllowedToExecuteFile to throw if called — builtins should
       // never reach the integrity check since they are compiled into the binary.
       t.mock.method(ci, ci.isAllowedToExecuteFile.name, () => {
         throw new Error('integrity check should not be called for builtins');
       });

       // These should load without triggering the integrity check
       assert.ok(require('node:fs'));
       assert.ok(require('path'));
     }
  );
});

describe('esm loader code integrity integration tests', async () => {
  it('should NOT throw an error if a file passes code integrity policy',
     async (t) => {
       t.mock.method(ci, ci.isAllowedToExecuteFile.name, () => { return true; });

       // This should import without throwing ERR_CODE_INTEGRITY_VIOLATION
       await import('../fixtures/code_integrity_test.js');
     }
  );

  it('should throw an error if a file does not pass code integrity policy',
     async (t) => {
       t.mock.method(ci, ci.isAllowedToExecuteFile.name, () => { return false; });
       try {
         await import('../fixtures/code_integrity_test2.js');
       } catch (e) {
         assert.strictEqual(e.code, 'ERR_CODE_INTEGRITY_VIOLATION');
         return;
       }

       assert.fail('No exception thrown');
     }
  );
});

describe('DisableInteractiveMode blocking tests', () => {
  const { spawnSync } = require('child_process');

  it('isInteractiveModeDisabled should return a boolean', () => {
    const result = spawnSync(process.execPath, [
      '--expose-internals',
      '-e',
      `
        const ci = require('internal/code_integrity');
        const disabled = ci.isInteractiveModeDisabled();
        if (typeof disabled !== 'boolean') process.exit(1);
      `,
    ]);
    assert.strictEqual(result.status, 0);
  });

  it('--eval should not be blocked when WDAC policy is not active', () => {
    const result = spawnSync(process.execPath, [
      '-e', 'console.log("ok")',
    ]);
    assert.strictEqual(result.status, 0);
    assert.strictEqual(result.stdout.toString().trim(), 'ok');
  });

  it('REPL should not be blocked when WDAC policy is not active', () => {
    const result = spawnSync(process.execPath, [
      '-i',
    ], { input: '.exit\n', timeout: 5000 });
    assert.strictEqual(result.status, 0);
  });

  it('stdin eval should not be blocked when WDAC policy is not active', () => {
    const result = spawnSync(process.execPath, [], {
      input: 'console.log("ok")\n',
      timeout: 5000,
    });
    assert.strictEqual(result.status, 0);
    assert.ok(result.stdout.toString().includes('ok'));
  });
});
