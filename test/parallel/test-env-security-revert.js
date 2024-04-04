'use strict';

require('../common');

const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const { writeFileSync } = require('node:fs');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const testCVE = 'CVE-2009-TEST';
const testVulnerabilityTitle = 'Non-existent vulnerability for testing';
const testCVEWarningMessage = `SECURITY WARNING: Reverting ${testCVE}: ${testVulnerabilityTitle}`;
const testProcKey = `REVERT_${testCVE.replace(/-/g, '_')}`;
const invalidCVE = 'CVE-2009-NOPE';
const invalidCVERegExp = new RegExp(`^.+: Error: Attempt to revert an unknown CVE \\[${invalidCVE}\\]$`);

const node = process.execPath;

function run({ env, dotenv, args, otherEnv }) {
  const dotenvPath = tmpdir.resolve('.env');
  if (dotenv != null) {
    writeFileSync(dotenvPath, `NODE_SECURITY_REVERT=${dotenv}\n`);
  }

  return spawnSync(node, [
    ...(dotenv != null ? ['--env-file', dotenvPath] : []),
    ...(args ?? []),
    '--print', `JSON.stringify([
      process.env.NODE_SECURITY_REVERT,
      Object.entries(process).filter(([name]) => name.startsWith("REVERT_")),
      child_process.execFileSync(${JSON.stringify(node)}, [
        '--print', 'process.env.NODE_SECURITY_REVERT'
      ]).toString().trim().split('\\n'),
    ])`,
  ], {
    env: {
      ...process.env,
      ...(otherEnv ?? {}),
      NODE_SECURITY_REVERT: env,
    },
    encoding: 'utf8'
  });
}

function expectSuccess(params) {
  const { status, stdout, stderr } = run(params);
  assert.strictEqual(status, 0, `${status} ${stderr}`);
  assert.strictEqual(stderr.length, 0, stderr);
  const lines = stdout.trim().split(/\r?\n/g);
  assert.notStrictEqual(lines.length, 0);
  const output = lines.pop();
  return [lines, JSON.parse(output)];
}

function expectError(params) {
  const { status, stdout, stderr } = run(params);
  assert.notStrictEqual(status, 0);
  return [stdout.trim(), stderr.trim()];
}

for (const method of ['env', 'dotenv']) {
  // Default: no security revert.
  assert.deepStrictEqual(expectSuccess({}), [[], [null, [], ['undefined']]]);

  // Revert a single CVE patch without child processes inheriting this behavior.
  assert.deepStrictEqual(expectSuccess({ [method]: testCVE }), [
    [testCVEWarningMessage],
    [null, [[testProcKey, true]], ['undefined']],
  ]);

  // Revert a single CVE patch with child processes inheriting this behavior.
  assert.deepStrictEqual(expectSuccess({ [method]: `${testCVE}+sticky` }), [
    [testCVEWarningMessage],
    [`${testCVE}+sticky`, [[testProcKey, true]], [testCVEWarningMessage, `${testCVE}+sticky`]],
  ]);

  // Try to revert a CVE patch that does not exist.
  for (const suffix of ['', '+sticky']) {
    const [stdout, stderr] = expectError({ [method]: `${invalidCVE}${suffix}` });
    assert.strictEqual(stdout, '');
    assert.match(stderr, invalidCVERegExp);
  }

  // Try to revert two CVE patches, one of which does not exist.
  for (const suffix of ['', '+sticky']) {
    const [stdout, stderr] = expectError({ [method]: `${testCVE},${invalidCVE}${suffix}` });
    assert.strictEqual(stdout, testCVEWarningMessage);
    assert.match(stderr, invalidCVERegExp);
  }

  // The command-line argument should take precedence over the environment variable
  // and is never inherited by child processes.
  assert.deepStrictEqual(expectSuccess({
    [method]: invalidCVE,
    args: ['--security-revert', testCVE],
  }), [
    [testCVEWarningMessage],
    [null, [[testProcKey, true]], ['undefined']],
  ]);
}

// The environment variable should take precedence over a dotenv file.
assert.deepStrictEqual(expectSuccess({
  env: testCVE,
  dotenv: invalidCVE
}), [
  [testCVEWarningMessage],
  [null, [[testProcKey, true]], ['undefined']],
]);

// We don't want security reverts to be inherited implicitly, thus, neither
// --security-revert nor --env-file should be allowed in NODE_OPTIONS.
for (const NODE_OPTIONS of [
  `--env-file=${tmpdir.resolve('.env')}`,
  `--security-revert=${testCVE}`,
]) {
  const [stdout, stderr] = expectError({
    otherEnv: { NODE_OPTIONS }
  });
  assert.strictEqual(stdout, '');
  const optPrefix = NODE_OPTIONS.substring(0, NODE_OPTIONS.indexOf('=') + 1);
  assert.match(stderr, new RegExp(`^.+: ${optPrefix} is not allowed in NODE_OPTIONS$`));
}
