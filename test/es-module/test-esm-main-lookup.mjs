// Flags: --experimental-modules
/* eslint-disable node-core/required-modules */
import '../common/index.mjs';
import assert from 'assert';

async function main() {
  let mod;
  try {
    mod = await import('../fixtures/es-modules/pjson-main');
  } catch (e) {
    assert.strictEqual(e.code, 'MODULE_NOT_FOUND');
  }

  assert.strictEqual(mod, undefined);

  try {
    mod = await import('../fixtures/es-modules/pjson-main/main.mjs');
  } catch (e) {
    console.log(e);
    assert.fail();
  }

  assert.strictEqual(mod.main, 'main');
}

main();
