import { expectsError, mustCall } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';

import assert from 'assert';
import { readJSON, readJSONSync } from 'fs';
import { open, readJSON as readJSONPromises } from 'fs/promises';

const invalidJsonFile = fixtures.path('invalid.json');
const validJsonFile = fixtures.path('experimental.json');
const expectedValid = { ofLife: 42 };
const invalidPath = fixtures.path('i/do/not/exist');

function reviver(key, value) {
  if (key === 'ofLife') {
    return value / 2;
  }
  return value;
}
const expectedWithReviver = { ofLife: 21 };

// fs.readJSON()
{
  readJSON(
    validJsonFile,
    mustCall((error, json) => {
      assert.strictEqual(error, null);
      assert.deepStrictEqual(json, expectedValid);
    })
  );

  readJSON(
    validJsonFile,
    { reviver },
    mustCall((error, json) => {
      assert.strictEqual(error, null);
      assert.deepStrictEqual(json, expectedWithReviver);
    })
  );

  readJSON(
    invalidJsonFile,
    expectsError({
      name: 'SyntaxError',
    })
  );

  readJSON(
    invalidPath,
    expectsError({
      code: 'ENOENT',
    })
  );

  assert.throws(() => readJSON(null, () => {}), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  assert.throws(() => readJSON(validJsonFile), {
    code: 'ERR_INVALID_CALLBACK',
  });
  assert.throws(() => readJSON(validJsonFile, { reviver: true }, () => {}), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
} // fs.readJSON()

// fs.readJSONSync()
{
  assert.deepStrictEqual(readJSONSync(validJsonFile), expectedValid);

  assert.deepStrictEqual(
    readJSONSync(validJsonFile, { reviver }),
    expectedWithReviver
  );

  assert.throws(() => readJSONSync(invalidJsonFile), { name: 'SyntaxError' });

  assert.throws(() => readJSONSync(invalidPath), { code: 'ENOENT' });

  assert.throws(() => readJSONSync(null), { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => readJSONSync(validJsonFile, { reviver: true }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
} // fs.readJSONSync()

// fsPromises.readJSON()
{
  assert.deepStrictEqual(await readJSONPromises(validJsonFile), expectedValid);

  assert.deepStrictEqual(
    await readJSONPromises(validJsonFile, { reviver }),
    expectedWithReviver
  );

  await assert.rejects(readJSONPromises(invalidJsonFile), {
    name: 'SyntaxError',
  });

  await assert.rejects(readJSONPromises(invalidPath), { code: 'ENOENT' });

  await assert.rejects(readJSONPromises(null), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  await assert.rejects(readJSONPromises(validJsonFile, { reviver: true }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
} // fsPromises.readJSON()

// fileHandle.readJSON()
{
  let fileHandle = await open(validJsonFile);
  assert.deepStrictEqual(await fileHandle.readJSON(), expectedValid);
  await fileHandle.close();

  fileHandle = await open(invalidJsonFile);
  await assert.rejects(fileHandle.readJSON(), { name: 'SyntaxError' });
  await fileHandle.close();
} // fileHandle.readJSON()
