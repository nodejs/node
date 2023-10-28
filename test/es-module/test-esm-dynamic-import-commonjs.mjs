import '../common/index.mjs';
import tmpdir from '../common/tmpdir.js';
import assert from 'node:assert';
import fs from 'node:fs/promises';

tmpdir.refresh();

const commonjsURL = tmpdir.fileURL('commonjs-module-2.cjs');

await fs.writeFile(commonjsURL, '');

let tickDuringCJSImport = false;
process.nextTick(() => { tickDuringCJSImport = true; });
await import(commonjsURL);

assert(!tickDuringCJSImport);
