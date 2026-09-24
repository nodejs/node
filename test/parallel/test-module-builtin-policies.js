// Flags: --expose-internals
'use strict';

// Run before loading common, whose async-hooks checks need --expose-internals.
// Child processes and Workers exercise only the public loaders.
if (process.argv[2] === 'child') {
  const policies = JSON.parse(process.argv[3]);
  const enabled = process.argv[4] === 'true';
  Promise.all(policies.map((policy) => checkPolicy(policy, enabled)))
    .catch((err) => {
      console.error(err);
      process.exitCode = 1;
    });
  return;
}

const common = require('../common');
const assert = require('assert');
const { isBuiltin } = require('module');
const { Worker } = require('worker_threads');
const { spawnSyncAndAssert } = require('../common/child_process');
const {
  internalBinding,
  getBuiltinModulePolicies,
} = require('internal/test/binding');
const { getCLIOptionsInfo } = require('internal/options');

const policies = getBuiltinModulePolicies();
const { builtinIds } = internalBinding('builtins');
const { options } = getCLIOptionsInfo();
const moduleAvailability = new Map([
  ['dtls', common.hasDtls],
  ['ffi', common.hasFFI],
  ['quic', common.hasQuic],
  ['sqlite', common.hasSQLite],
]);

// Validate every declared policy before testing its behavior.
{
  const seenIds = new Set();
  for (const policy of policies) {
    assert(Array.isArray(policy));
    assert.strictEqual(policy.length, 3);
    const [id, schemeOnly, option] = policy;
    assert.strictEqual(typeof id, 'string');
    assert(!id.startsWith('internal/'), id);
    assert(builtinIds.includes(id), `Unknown builtin: ${id}`);
    assert(!seenIds.has(id), `Duplicate policy: ${id}`);
    seenIds.add(id);
    assert.strictEqual(typeof schemeOnly, 'boolean', id);

    if (option === null) continue;
    assert.match(option, /^--experimental-[a-z-]+$/);

    // FFI has no CLI option in builds without FFI support.
    const missingFFIOption = id === 'ffi' && !common.hasFFI &&
      option === '--experimental-ffi';
    assert(options.has(option) || missingFFIOption,
           `Unknown builtin option: ${option}`);
  }
}

// Callers must not be able to change the loader's policy through this API.
{
  const copy = getBuiltinModulePolicies();
  copy[0][0] = 'changed';
  copy.pop();
  assert.deepStrictEqual(getBuiltinModulePolicies(), policies);
}

// Test defaults, command-line flags, NODE_OPTIONS, and their precedence in
// fresh processes.
for (const policy of policies) {
  const [id, , option] = policy;
  if (moduleAvailability.get(id) === false) continue;

  runPolicyTest(policy, [], option === null || options.get(option).defaultIsTrue);
  if (option !== null) {
    const disabledOption = option.replace('--', '--no-');
    runPolicyTest(policy, [option], true);
    runPolicyTest(policy, [disabledOption], false);

    if (!process.config.variables.node_without_node_options) {
      runPolicyTest(policy, [], true, option);
      runPolicyTest(policy, [], false, disabledOption);
      // Command-line options take precedence over NODE_OPTIONS.
      runPolicyTest(policy, [option], true, disabledOption);
      runPolicyTest(policy, [disabledOption], false, option);
    }
  }
}

// Test option-gated builtins in Workers without changing the parent state.
{
  const policiesByOption = new Map();
  for (const policy of policies) {
    const [id, , option] = policy;
    if (option === null || moduleAvailability.get(id) === false) continue;

    const optionPolicies = policiesByOption.get(option);
    if (optionPolicies === undefined) {
      policiesByOption.set(option, [policy]);
    } else {
      optionPolicies.push(policy);
    }
  }

  // Test each option once, together with all builtins it controls.
  for (const [option, optionPolicies] of policiesByOption) {
    const parentState = optionPolicies.map(([id]) => [
      id,
      isBuiltin(`node:${id}`),
    ]);
    const disabledOption = option.replace('--', '--no-');

    for (const enabled of [true, false]) {
      const worker = new Worker(__filename, {
        argv: ['child', JSON.stringify(optionPolicies), String(enabled)],
        execArgv: [enabled ? option : disabledOption],
        env: { ...process.env, NODE_OPTIONS: '' },
      });

      worker.on('exit', common.mustCall((code) => {
        assert.strictEqual(code, 0);
        for (const [id, parentEnabled] of parentState) {
          assert.strictEqual(isBuiltin(`node:${id}`), parentEnabled, id);
        }
      }));
    }
  }
}

// Run in a fresh process so each case initializes the loader with its flags.
function runPolicyTest(policy, flags, enabled, nodeOptions = '') {
  spawnSyncAndAssert(process.execPath, [
    ...flags,
    __filename,
    'child',
    JSON.stringify([policy]),
    String(enabled),
  ], { env: { ...process.env, NODE_OPTIONS: nodeOptions } }, { status: 0 });
}

async function checkPolicy([id, schemeOnly], enabled) {
  const assert = require('assert');
  const { builtinModules, isBuiltin } = require('module');
  const prefixed = `node:${id}`;
  assert.strictEqual(builtinModules.includes(id), enabled && !schemeOnly, id);
  assert.strictEqual(builtinModules.includes(prefixed),
                     enabled && schemeOnly, prefixed);

  for (const specifier of [id, prefixed]) {
    const supported = enabled && (!schemeOnly || specifier === prefixed);
    assert.strictEqual(isBuiltin(specifier), supported, specifier);
    if (supported) {
      const exports = require(specifier);
      assert.strictEqual(process.getBuiltinModule(specifier), exports);
      assert.strictEqual((await import(specifier)).default, exports);
    } else {
      assert.strictEqual(process.getBuiltinModule(specifier), undefined);
      assert.throws(() => require(specifier), {
        code: specifier === prefixed ?
          'ERR_UNKNOWN_BUILTIN_MODULE' : 'MODULE_NOT_FOUND',
      });
      await assert.rejects(import(specifier), {
        code: specifier === prefixed ?
          'ERR_UNKNOWN_BUILTIN_MODULE' : 'ERR_MODULE_NOT_FOUND',
      });
    }
  }
}
