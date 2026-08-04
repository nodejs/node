// Test fixture that throws when source phase not defined for dynamic import
import { ok, strictEqual, rejects } from 'node:assert';
import { fileURLToPath } from 'node:url';
import { pathToFileURL } from 'node:url';

const fileUrl = pathToFileURL(fileURLToPath(new URL('./wasm-source-phase.js', import.meta.url))).href;
await rejects(import.source(fileUrl), (e) => {
  strictEqual(e instanceof SyntaxError, true);
  strictEqual(e.message.includes('Source phase import object is not defined for module'), true);
  strictEqual(e.message.includes(fileUrl), true);
  return true;
});
