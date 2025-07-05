'use strict';

const common = require('../common');
const assert = require('node:assert');
const { describe, it } = require('node:test');

const envFilePath = '../fixtures/dotenv/sections.env';

async function getProcessEnvTestEntries(sections) {
  const code = `
      process.loadEnvFile('${envFilePath}', { sections: ${JSON.stringify(sections)} });
      console.log(
        JSON.stringify(
          Object.fromEntries(Object.entries(process.env).filter(([key]) => key.startsWith('_ENV_TEST_')))
        )
      );
    `.trim();
  const child = await common.spawnPromisified(
    process.execPath,
    ['--eval', code],
    { cwd: __dirname },
  );
  assert.strictEqual(child.code, 0);
  return JSON.parse(child.stdout.replace(/\n/g, ''));
}

describe('process.loadEnvFile() with sections', () => {
  it('should allow the selection of sections', async () => {
    const env = await getProcessEnvTestEntries(['dev']);
    assert.deepStrictEqual(env, {
      _ENV_TEST_A: 'A (development)',
      _ENV_TEST_B: 'B (top-level)',
      _ENV_TEST_C: 'C (development)',
    });
  });

  it('should ignore sections when they are omitted', async () => {
    const env = await getProcessEnvTestEntries([]);
    assert.deepStrictEqual(env, {
      _ENV_TEST_A: 'A (top-level)',
      _ENV_TEST_B: 'B (top-level)',
    });
  });

  it('should throw an ERR_INVALID_ARG_TYPE if a non-array sections value is passed', async () => {
    let error;
    try {
      process.loadEnvFile(envFilePath, { sections: () => console.log('this is invalid') });
    } catch (e) {
      error = e;
    }
    assert.strictEqual(error.code, 'ERR_INVALID_ARG_TYPE');
  });

  it('should throw an ERR_INVALID_ARG_TYPE if an array not containing all strings is passed', async () => {
    let error;
    try {
      process.loadEnvFile(envFilePath, { sections: ['a', 'b', 1, () => 2] });
    } catch (e) {
      error = e;
    }
    assert.strictEqual(error.code, 'ERR_INVALID_ARG_TYPE');
  });
});
