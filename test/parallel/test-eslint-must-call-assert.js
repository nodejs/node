'use strict';
const common = require('../common');
if ((!common.hasCrypto) || (!common.hasIntl)) {
  common.skip('ESLint tests require crypto and Intl');
}
common.skipIfEslintMissing();

const RuleTester = require('../../tools/eslint/node_modules/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/must-call-assert');

const message = 'Assertions must be wrapped into `common.mustCall` or `common.mustCallAtLeast`';

const tester = new RuleTester();
tester.run('must-call-assert', rule, {
  valid: [
    'assert.strictEqual(2+2, 4)',
    'process.on("message", common.mustCallAtLeast((code) => {assert.strictEqual(code, 0)}));',
    'process.once("message", common.mustCall((code) => {assert.strictEqual(code, 0)}));',
    'process.once("message", common.mustCall((code) => {if(2+2 === 5) { assert.strictEqual(code, 0)} }));',
    'process.once("message", common.mustCall((code) => { (() => assert.strictEqual(code, 0))(); }));',
    '(async () => {await assert.rejects(fun())})().then()',
    '[1, true].forEach((val) => assert.strictEqual(fun(val), 0));',
    'const assert = require("node:assert")',
    'const assert = require("assert")',
    'const assert = require("assert/strict")',
    'const assert = require("node:assert/strict")',
    'import assert from "assert"',
    'import * as assert from "assert"',
    'import assert from "assert/strict"',
    'import * as assert from "assert/strict"',
    'import assert from "node:assert"',
    'import * as assert from "node:assert"',
    'import assert from "node:assert/strict"',
    'import * as assert from "node:assert/strict"',
    `
    assert.throws(() => {}, (err) => {
      assert.strictEqual(err, 5);
    });
    process.on('exit', () => {
      assert.ok();
    });
    process.once('exit', () => {
      assert.ok();
    });
    process.on('message', () => {
      assert.fail('error message');
    });
    Promise.resolve().then((arg) => {
      assert.ok(arg);
    }).then(common.mustCall());
    new Promise(() => {
      assert.ok(global.prop);
    }).then(common.mustCall());
    process.nextTick(() => {
      assert.ok(String);
    });
    `,
    `
    import test from 'node:test';
    import assert from 'node:assert';

    test("whatever", () => {
      assert.strictEqual(2+2, 5);
    });
    `,
    `
    import test from 'node:test';
    import assert from 'node:assert';

    describe("whatever", () => {
      it("should not be reported", async () => {
        assert.strictEqual(2+2, 5);
      });
    });
    `,
  ],
  invalid: [
    {
      code: 'process.on("message", (code) => assert.strictEqual(code, 0))',
      errors: [{ message }],
    },
    {
      code: `
      process.once("message", () => {
        process.once("message", common.mustCall((code) => {
          assert.strictEqual(code, 0);
        }));
      });
      `,
      errors: [{ message }],
    },
    {
      code: 'function test() { process.on("message", (code) => assert.strictEqual(code, 0)) }',
      errors: [{ message }],
    },
    {
      code: 'process.once("message", (code) => {if(2+2 === 5) { assert.strictEqual(code, 0)} });',
      errors: [{ message }],
    },
    {
      code: 'process.once("message", (code) => { (() => { assert.strictEqual(code, 0)})(); });',
      errors: [{ message }],
    },
    {
      code: 'child.once("exit", common.mustCall((code) => {setImmediate(() => { assert.strictEqual(code, 0)}); }));',
      errors: [{ message }],
    },
    {
      code: 'require("node:assert").strictEqual(2+2, 5)',
      errors: [{ message: 'Only assign `node:assert` to `assert`' }],
    },
    {
      code: 'const { strictEqual } = require("node:assert")',
      errors: [{ message: 'Only assign `node:assert` to `assert`' }],
    },
    {
      code: 'const { strictEqual } = require("node:assert/strict")',
      errors: [{ message: 'Only assign `node:assert` to `assert`' }],
    },
    {
      code: 'const { strictEqual } = require("assert")',
      errors: [{ message: 'Only assign `node:assert` to `assert`' }],
    },
    {
      code: 'const { strictEqual } = require("assert/strict")',
      errors: [{ message: 'Only assign `node:assert` to `assert`' }],
    },
    {
      code: 'const someOtherName = require("assert")',
      errors: [{ message: 'Only assign `node:assert` to `assert`' }],
    },
    {
      code: 'import assert, { strictEqual } from "assert"',
      errors: [{ message: 'Only assign `node:assert` to `assert`' }],
    },
    {
      code: 'import * as someOtherName from "assert"',
      errors: [{ message: 'Only assign `node:assert` to `assert`' }],
    },
    {
      code: 'import someOtherName from "assert"',
      errors: [{ message: 'Only assign `node:assert` to `assert`' }],
    },
    {
      code: 'import "assert"',
      errors: [{ message: 'Only assign `node:assert` to `assert`' }],
    },
    {
      code: 'import { strictEqual } from "node:assert"',
      errors: [{ message: 'Only assign `node:assert` to `assert`' }],
    },
    {
      code: 'import assert, { strictEqual } from "node:assert"',
      errors: [{ message: 'Only assign `node:assert` to `assert`' }],
    },
    {
      code: 'import * as someOtherName from "node:assert"',
      errors: [{ message: 'Only assign `node:assert` to `assert`' }],
    },
    {
      code: 'import someOtherName from "node:assert"',
      errors: [{ message: 'Only assign `node:assert` to `assert`' }],
    },
    {
      code: 'import "node:assert"',
      errors: [{ message: 'Only assign `node:assert` to `assert`' }],
    },
  ]
});
