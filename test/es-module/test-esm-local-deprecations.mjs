import { mustCall } from '../common/index.mjs';
import assert from 'assert';
import fixtures from '../common/fixtures.js';
import { pathToFileURL } from 'url';

const selfDeprecatedFolders =
    fixtures.path('/es-modules/self-deprecated-folders/main.js');

let curWarning = 0;
const expectedWarnings = [
  '"./" in the "exports" field',
  '"#self/" in the "imports" field'
];

process.addListener('warning', mustCall((warning) => {
  assert(warning.stack.includes(expectedWarnings[curWarning++]), warning.stack);
}, expectedWarnings.length));

(async () => {
  await import(pathToFileURL(selfDeprecatedFolders));
})()
.catch((err) => console.error(err));
