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
  it('should throw an error if a .node file does not pass code integrity policy',
     (t) => {
       t.mock.method(ci, ci.isAllowedToExecuteFile.name, () => { return false; });

       assert.throws(
         () => {
           require('../fixtures/code_integrity_test.node');
         },
         {
           code: 'ERR_CODE_INTEGRITY_VIOLATION',
         },
       );
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
