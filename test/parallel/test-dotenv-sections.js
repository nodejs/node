'use strict';

const common = require('../common');
const assert = require('node:assert');
const { describe, it } = require('node:test');

const envFilePath = '../fixtures/dotenv/sections.env';

async function getProcessEnvTestEntries(envFileArg) {
  const code = `
      console.log(
        JSON.stringify(
          Object.fromEntries(Object.entries(process.env).filter(([key]) => key.startsWith('_ENV_TEST_')))
        )
      );
    `.trim();
  const child = await common.spawnPromisified(
    process.execPath,
    [ `--env-file=${envFileArg}`, '--eval', code ],
    { cwd: __dirname },
  );
  assert.strictEqual(child.code, 0);
  return JSON.parse(child.stdout.replace(/\n/g, ''));
}

describe('.env sections support', () => {
  it('should only get the top-level variables if a section is not specified', async () => {
    const env = await getProcessEnvTestEntries(envFilePath);
    assert.deepStrictEqual(env, {
      _ENV_TEST_A: 'A (top-level)',
      _ENV_TEST_B: 'B (top-level)',
    });
  });

  it('should get section specific variables if a section is specified', async () => {
    const env = await getProcessEnvTestEntries(`${envFilePath}#dev`);
    assert.strictEqual(env._ENV_TEST_A, 'A (development)');
    assert.strictEqual(env._ENV_TEST_C, 'C (development)');
    assert(!('_ENV_TEST_D' in env), 'the _ENV_TEST_D should not be present for the dev section');
  });

  it('should allow top-level variables to be inherited if not specified in a section', async () => {
    const env = await getProcessEnvTestEntries(`${envFilePath}#dev`);
    assert.strictEqual(env._ENV_TEST_B, 'B (top-level)');
  });

  it('should allow multiple sections to be specified (values are overridden as per the file order)', async () => {
    const env = await getProcessEnvTestEntries(`${envFilePath}#dev#prod`);
    assert.deepStrictEqual(env, {
      _ENV_TEST_A: 'A (production)',
      _ENV_TEST_B: 'B (top-level)',
      _ENV_TEST_C: 'C (development)',
      _ENV_TEST_D: 'D (production)'
    });
  });

  it('should ignore section aliases', async () => {
    const env = await getProcessEnvTestEntries(`${envFilePath}#x.y.z`);
    assert.strictEqual(env._ENV_TEST_X, 'Y_Z');
  });

  it('should allow comments to be present after the section declaration', async () => {
    const env = await getProcessEnvTestEntries(`${envFilePath}#with_comment`);
    assert.strictEqual(env._ENV_TEST_COMMENT, 'this section has a comment');
  });
});
