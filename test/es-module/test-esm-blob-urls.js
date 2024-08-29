'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const { test } = require('node:test');

if (!common.hasCrypto) common.skip('missing crypto');

async function createURL(mime, body) {
  const blob = new Blob([body], { type: mime });
  return URL.createObjectURL(blob);
}

function createBase64URL(mime, body) {
  return `data:${mime};base64,${Buffer.from(body).toString('base64')}`;
}

test('importing JavaScript from a Blob: URL should work', async (t) => {
  const url = await createURL('text/javascript', 'export default { "key": "value" }');
  const { default: expected } = await import(url);
  t.assert.deepStrictEqual(expected, { key: 'value' });
});

test('import.meta.url should be equal to the blob URL', async (t) => {
  const url = await createURL('text/javascript', 'export default import.meta.url;');
  const { default: expected } = await import(url);
  t.assert.deepStrictEqual(expected, url);
});


test('importing JSON from a Blob: URL should work', async (t) => {
  const url = await createURL('application/json', '{ "key": "value" }');
  const { default: expected } = await import(url, { with: { type: 'json' } });
  t.assert.deepStrictEqual(expected, { key: 'value' });
});

test('importing a module from within a blob: URL should work', async (t) => {
  await t.test('file: URLs', async (t) => {
    const url = await createURL('text/javascript', `import { message } from ${JSON.stringify(fixtures.fileURL('es-modules/message.mjs'))}; export default message;`);
    const { default: expected } = await import(url);
    t.assert.strictEqual(expected, 'A message');
  });

  await t.test('data: URLs', async (t) => {
    const dataURL = createBase64URL('text/javascript', 'export const message = "Base64 Imported";');
    const url = await createURL('text/javascript', `import { message } from "${dataURL}"; export default message;`);
    const { default: expected } = await import(url);
    t.assert.strictEqual(expected, 'Base64 Imported');
  });

  await t.test('blob: URLs', async (t) => {
    const url = await createURL('text/javascript',
                                `const blob = new Blob(["export default 'Blob Imported'"], { type: 'application/javascript' });` +
      'const url = URL.createObjectURL(blob);' +
      'export default await import(url);'
    );
    const { default: { default: expected } } = await import(url);
    t.assert.strictEqual(expected, 'Blob Imported');
  });
});

test('importing a disallowed mimetype should fail', async (t) => {
  const url = await createURL('invalid/mime', '');
  await t.assert.rejects(import(url), /ERR_UNKNOWN_MODULE_FORMAT/);
});

test('importing an invalid blob should fail', async (t) => {
  await t.assert.rejects(import('blob:nodedata:123456'), /ERR_INVALID_URL/);
});
