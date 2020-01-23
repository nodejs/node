import { mustCall } from '../common/index.mjs';
import assert from 'assert';
import fixtures from '../common/fixtures.js';
import { spawn } from 'child_process';

const Export1 = fixtures.path('/es-modules/es-note-unexpected-export-1.cjs');
const Export2 = fixtures.path('/es-modules/es-note-unexpected-export-2.cjs');
const Import1 = fixtures.path('/es-modules/es-note-unexpected-import-1.cjs');
const Import2 = fixtures.path('/es-modules/es-note-promiserej-import-2.cjs');
const Import3 = fixtures.path('/es-modules/es-note-unexpected-import-3.cjs');
const Import4 = fixtures.path('/es-modules/es-note-unexpected-import-4.cjs');
const Import5 = fixtures.path('/es-modules/es-note-unexpected-import-5.cjs');

// Expect note to be included in the error output
const expectedNote = 'To load an ES module, ' +
'set "type": "module" in the package.json ' +
'or use the .mjs extension.';

const expectedCode = 1;

const pExport1 = spawn(process.execPath, [Export1]);
let pExport1Stderr = '';
pExport1.stderr.setEncoding('utf8');
pExport1.stderr.on('data', (data) => {
  pExport1Stderr += data;
});
pExport1.on('close', mustCall((code) => {
  assert.strictEqual(code, expectedCode);
  assert.ok(pExport1Stderr.includes(expectedNote),
            `${expectedNote} not found in ${pExport1Stderr}`);
}));


const pExport2 = spawn(process.execPath, [Export2]);
let pExport2Stderr = '';
pExport2.stderr.setEncoding('utf8');
pExport2.stderr.on('data', (data) => {
  pExport2Stderr += data;
});
pExport2.on('close', mustCall((code) => {
  assert.strictEqual(code, expectedCode);
  assert.ok(pExport2Stderr.includes(expectedNote),
            `${expectedNote} not found in ${pExport2Stderr}`);
}));

const pImport1 = spawn(process.execPath, [Import1]);
let pImport1Stderr = '';
pImport1.stderr.setEncoding('utf8');
pImport1.stderr.on('data', (data) => {
  pImport1Stderr += data;
});
pImport1.on('close', mustCall((code) => {
  assert.strictEqual(code, expectedCode);
  assert.ok(pImport1Stderr.includes(expectedNote),
            `${expectedNote} not found in ${pExport1Stderr}`);
}));

// Note this test shouldn't include the note
const pImport2 = spawn(process.execPath, [Import2]);
let pImport2Stderr = '';
pImport2.stderr.setEncoding('utf8');
pImport2.stderr.on('data', (data) => {
  pImport2Stderr += data;
});
pImport2.on('close', mustCall((code) => {
  // As this exits normally we expect 0
  assert.strictEqual(code, 0);
  assert.ok(!pImport2Stderr.includes(expectedNote),
            `${expectedNote} must not be included in ${pImport2Stderr}`);
}));

const pImport3 = spawn(process.execPath, [Import3]);
let pImport3Stderr = '';
pImport3.stderr.setEncoding('utf8');
pImport3.stderr.on('data', (data) => {
  pImport3Stderr += data;
});
pImport3.on('close', mustCall((code) => {
  assert.strictEqual(code, expectedCode);
  assert.ok(pImport3Stderr.includes(expectedNote),
            `${expectedNote} not found in ${pImport3Stderr}`);
}));


const pImport4 = spawn(process.execPath, [Import4]);
let pImport4Stderr = '';
pImport4.stderr.setEncoding('utf8');
pImport4.stderr.on('data', (data) => {
  pImport4Stderr += data;
});
pImport4.on('close', mustCall((code) => {
  assert.strictEqual(code, expectedCode);
  assert.ok(pImport4Stderr.includes(expectedNote),
            `${expectedNote} not found in ${pImport4Stderr}`);
}));

// Must exit with zero and show note
const pImport5 = spawn(process.execPath, [Import5]);
let pImport5Stderr = '';
pImport5.stderr.setEncoding('utf8');
pImport5.stderr.on('data', (data) => {
  pImport5Stderr += data;
});
pImport5.on('close', mustCall((code) => {
  assert.strictEqual(code, 0);
  assert.ok(!pImport5Stderr.includes(expectedNote),
            `${expectedNote} must not be included in ${pImport5Stderr}`);
}));
