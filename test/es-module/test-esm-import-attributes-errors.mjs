import '../common/index.mjs';
import assert from 'assert';

const jsModuleDataUrl = 'data:text/javascript,export{}';
const jsonModuleDataUrl = 'data:application/json,""';

await assert.rejects(
  // This rejects because of the unsupported MIME type, not because of the
  // unsupported assertion.
  import('data:text/css,', { with: { type: 'css' } }),
  { code: 'ERR_UNKNOWN_MODULE_FORMAT' }
);

await assert.rejects(
  import(`data:text/javascript,import${JSON.stringify(jsModuleDataUrl)}with{type:"json"}`),
  { code: 'ERR_IMPORT_ATTRIBUTE_TYPE_INCOMPATIBLE' }
);

await assert.rejects(
  import(jsModuleDataUrl, { with: { type: 'json' } }),
  { code: 'ERR_IMPORT_ATTRIBUTE_TYPE_INCOMPATIBLE' }
);

await assert.rejects(
  import(jsModuleDataUrl, { with: { type: 'json', other: 'unsupported' } }),
  { code: 'ERR_IMPORT_ATTRIBUTE_UNSUPPORTED' }
);

await assert.rejects(
  import(import.meta.url, { with: { type: 'unsupported' } }),
  { code: 'ERR_IMPORT_ATTRIBUTE_UNSUPPORTED' }
);

await assert.rejects(
  import(jsonModuleDataUrl),
  { code: 'ERR_IMPORT_ATTRIBUTE_MISSING' }
);

await assert.rejects(
  import(jsonModuleDataUrl, { with: {} }),
  { code: 'ERR_IMPORT_ATTRIBUTE_MISSING' }
);

await assert.rejects(
  import(jsonModuleDataUrl, { with: { foo: 'bar' } }),
  { code: 'ERR_IMPORT_ATTRIBUTE_UNSUPPORTED' }
);

await assert.rejects(
  import(jsonModuleDataUrl, { with: { type: 'unsupported' } }),
  { code: 'ERR_IMPORT_ATTRIBUTE_UNSUPPORTED' }
);

await assert.rejects(
  import(jsonModuleDataUrl, { assert: { type: 'json' } }),
  { code: 'ERR_IMPORT_ATTRIBUTE_MISSING' }
);

await assert.rejects(
  import(`data:text/javascript,import${JSON.stringify(jsonModuleDataUrl)}assert{type:"json"}`),
  SyntaxError
);
