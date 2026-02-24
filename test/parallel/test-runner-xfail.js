'use strict';
const common = require('../common');
const { test, suite } = require('node:test');
const { spawn } = require('child_process');
const assert = require('node:assert');

if (process.env.CHILD_PROCESS === 'true') {
  test('fail with message string', { expectFailure: 'reason string' }, () => {
    assert.fail('boom');
  });

  test('fail with label object', { expectFailure: { label: 'reason object' } }, () => {
    assert.fail('boom');
  });

  test('fail with match regex', { expectFailure: { match: /boom/ } }, () => {
    assert.fail('boom');
  });

  test('fail with match object', { expectFailure: { match: { message: 'boom' } } }, () => {
    assert.fail('boom');
  });

  test('fail with match class', { expectFailure: { match: assert.AssertionError } }, () => {
    assert.fail('boom');
  });

  test('fail with match error (wrong error)', { expectFailure: { match: /bang/ } }, () => {
    assert.fail('boom'); // Should result in real failure because error doesn't match
  });

  test('unexpected pass', { expectFailure: true }, () => {
    // Should result in real failure because it didn't fail
  });

  test('fail with empty string', { expectFailure: '' }, () => {
    assert.fail('boom');
  });

  // 1. Matcher: RegExp
  test('fails with regex matcher', { expectFailure: /expected error/ }, () => {
    throw new Error('this is the expected error');
  });

  test('fails with regex matcher (mismatch)', { expectFailure: /expected error/ }, () => {
    throw new Error('wrong error'); // Should fail the test
  });

  // 2. Matcher: Class
  test('fails with class matcher', { expectFailure: RangeError }, () => {
    throw new RangeError('out of bounds');
  });

  test('fails with class matcher (mismatch)', { expectFailure: RangeError }, () => {
    throw new TypeError('wrong type'); // Should fail the test
  });

  // 3. Matcher: Function
  test('fails with function matcher', {
    expectFailure: (err) => err.code === 'ERR_FUNCTION',
  }, () => {
    const err = new Error('boom');
    err.code = 'ERR_FUNCTION';
    throw err;
  });

  test('fails with function matcher (mismatch)', {
    expectFailure: (err) => err.code === 'ERR_FUNCTION',
  }, () => {
    const err = new Error('boom');
    err.code = 'ERR_WRONG';
    throw err; // Should fail
  });

  // 4. Matcher: Object (Properties)
  test('fails with object matcher', { expectFailure: { code: 'ERR_TEST' } }, () => {
    const err = new Error('boom');
    err.code = 'ERR_TEST';
    throw err;
  });

  test('fails with object matcher (mismatch)', { expectFailure: { code: 'ERR_TEST' } }, () => {
    const err = new Error('boom');
    err.code = 'ERR_WRONG';
    throw err; // Should fail
  });

  // 5. Configuration Object: Reason + Validation
  test('fails with config object (label + match)', {
    expectFailure: {
      label: 'Bug #124',
      match: /boom/
    }
  }, () => {
    throw new Error('boom');
  });

  test('fails with config object (label only)', {
    expectFailure: { label: 'Bug #125' }
  }, () => {
    throw new Error('boom');
  });

  test('fails with config object (match only)', {
    expectFailure: { match: /boom/ }
  }, () => {
    throw new Error('boom');
  });

  // 6. Edge Case: Empty Object (Should throw ERR_INVALID_ARG_VALUE during creation)
  test('invalid empty object throws', () => {
    // eslint-disable-next-line no-restricted-syntax
    assert.throws(() => test('invalid empty object', { expectFailure: {} }, () => {}));
  });

  // 7. Primitives and Truthiness
  test('fails with boolean true', { expectFailure: true }, () => {
    throw new Error('any error');
  });

  // 8. Unexpected Pass (Enhanced)
  test('unexpected pass (reason string)', { expectFailure: 'should fail' }, () => {
    // Pass
  });

  test('unexpected pass (matcher)', { expectFailure: /boom/ }, () => {
    // Pass
  });

  // 9. Test level expectFailure with subtests
  test('test level expectFailure with subtests', { expectFailure: true }, async (t) => {
    await t.test('subtest should pass if it throws', () => {
      throw new Error('fail');
    });

    await t.test('subtest should fail if it does not throw', () => {
      // This should fail the test because the suite expects failure,
      // and this subtest inherited that expectation but didn't throw.
    });
  });

  test('test level expectFailure with subtests and matcher', { expectFailure: /suite error/ }, async (t) => {
    await t.test('subtest match suite expectation', () => {
      throw new Error('suite error');
    });
  });

  // 10. Suite level expectFailure
  suite('suite level expectFailure', { expectFailure: true }, () => {
    test('suite subtest should pass if it throws', () => {
      throw new Error('fail');
    });

    test('suite subtest should fail if it does not throw', () => {
      // This should fail the test because the suite expects failure,
      // and this subtest inherited that expectation but didn't throw.
    });
  });

  suite('suite level expectFailure with matcher', { expectFailure: /suite error/ }, () => {
    test('suite subtest match suite expectation', () => {
      throw new Error('suite error');
    });
  });

  test('correct config with label and match object', {
    expectFailure: {
      label: 'Bug #124',
      match: { code: 'ERR_TEST' }
    }
  }, () => {
    const err = new Error('boom');
    err.code = 'ERR_TEST';
    throw err;
  });

  test('fails with ambiguous config (mixed props)', {
    expectFailure: {
      code: 'ERR_TEST',
      label: 'Bug #124',
    }
  }, () => {
    const err = new Error('boom');
    err.code = 'ERR_TEST';
    throw err;
  });

  test('fails with ambiguous config (mixed props with match)', {
    expectFailure: {
      code: 'ERR_TEST',
      match: /message/
    }
  }, () => {
    const err = new Error('message');
    err.code = 'ERR_TEST';
    throw err;
  });

} else {
  const child = spawn(process.execPath, ['--test-reporter', 'tap', __filename], {
    env: { ...process.env, CHILD_PROCESS: 'true' },
    stdio: 'pipe',
  });

  let stdout = '';
  child.stdout.setEncoding('utf8');
  child.stdout.on('data', (chunk) => { stdout += chunk; });

  child.on('close', common.mustCall((code) => {
    // We expect exit code 1 because 'unexpected pass' and 'wrong error' should fail the test run
    assert.strictEqual(code, 1);

    assert.match(stdout, /ok \d+ - fail with message string # EXPECTED FAILURE reason string/);
    assert.match(stdout, /ok \d+ - fail with label object # EXPECTED FAILURE reason object/);
    assert.match(stdout, /ok \d+ - fail with match regex # EXPECTED FAILURE/);
    assert.match(stdout, /ok \d+ - fail with match object # EXPECTED FAILURE/);
    assert.match(stdout, /ok \d+ - fail with match class # EXPECTED FAILURE/);
    assert.match(stdout, /not ok \d+ - fail with match error \(wrong error\) # EXPECTED FAILURE/);
    assert.match(stdout, /not ok \d+ - unexpected pass # EXPECTED FAILURE/);
    assert.match(stdout, /ok \d+ - fail with empty string # EXPECTED FAILURE/);

    // New tests verification
    assert.match(stdout, /ok \d+ - fails with regex matcher # EXPECTED FAILURE/);
    assert.match(stdout, /not ok \d+ - fails with regex matcher \(mismatch\) # EXPECTED FAILURE/);
    assert.match(stdout, /ok \d+ - fails with class matcher # EXPECTED FAILURE/);
    assert.match(stdout, /not ok \d+ - fails with class matcher \(mismatch\) # EXPECTED FAILURE/);
    assert.match(stdout, /ok \d+ - fails with function matcher # EXPECTED FAILURE/);
    assert.match(stdout, /not ok \d+ - fails with function matcher \(mismatch\) # EXPECTED FAILURE/);
    assert.match(stdout, /ok \d+ - fails with object matcher # EXPECTED FAILURE/);
    assert.match(stdout, /not ok \d+ - fails with object matcher \(mismatch\) # EXPECTED FAILURE/);
    assert.match(stdout, /ok \d+ - fails with config object \(label \+ match\) # EXPECTED FAILURE Bug \\#124/);
    assert.match(stdout, /ok \d+ - fails with config object \(label only\) # EXPECTED FAILURE Bug \\#125/);
    assert.match(stdout, /ok \d+ - fails with config object \(match only\) # EXPECTED FAILURE/);
    assert.match(stdout, /ok \d+ - fails with boolean true # EXPECTED FAILURE/);
    assert.match(stdout, /not ok \d+ - unexpected pass \(reason string\) # EXPECTED FAILURE should fail/);
    assert.match(stdout, /not ok \d+ - unexpected pass \(matcher\) # EXPECTED FAILURE/);

    // Suite inheritance verification
    assert.match(stdout, /ok \d+ - test level expectFailure with subtests # EXPECTED FAILURE/);
    assert.match(stdout, /ok \d+ - subtest should pass if it throws # EXPECTED FAILURE/);
    assert.match(stdout, /not ok \d+ - subtest should fail if it does not throw # EXPECTED FAILURE/);
    assert.match(stdout, /ok \d+ - test level expectFailure with subtests and matcher # EXPECTED FAILURE/);
    assert.match(stdout, /ok \d+ - subtest match suite expectation # EXPECTED FAILURE/);

    // Suite level expectFailure verification
    assert.match(stdout, /ok \d+ - suite level expectFailure # EXPECTED FAILURE/);
    assert.match(stdout, /ok \d+ - suite subtest should pass if it throws # EXPECTED FAILURE/);
    assert.match(stdout, /not ok \d+ - suite subtest should fail if it does not throw # EXPECTED FAILURE/);
    assert.match(stdout, /ok \d+ - suite level expectFailure with matcher # EXPECTED FAILURE/);
    assert.match(stdout, /ok \d+ - suite subtest match suite expectation # EXPECTED FAILURE/);

    // Empty object error
    assert.match(stdout, /ok \d+ - invalid empty object throws/);
    assert.match(stdout, /ok \d+ - correct config with label and match object # EXPECTED FAILURE Bug \\#124/);
    assert.match(stdout, /not ok \d+ - fails with ambiguous config \(mixed props\) # EXPECTED FAILURE/);
    assert.match(stdout, /not ok \d+ - fails with ambiguous config \(mixed props with match\) # EXPECTED FAILURE/);

  }));
}
