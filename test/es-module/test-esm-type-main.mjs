import '../common/index.mjs';
import assert from 'assert';
import { importFixture } from '../fixtures/pkgexports.mjs';

// DEP0151 End-of-Life: "type": "module" with a "main" field that omits
// the file extension no longer resolves.
await assert.rejects(importFixture('type-main'), {
  code: 'ERR_INVALID_PACKAGE_CONFIG',
  message: /Automatic extension resolution of the "main" field is not supported/,
});
