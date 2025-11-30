// Test fixture that source phase imports don't execute
import { strictEqual, rejects } from 'node:assert';
import { fileURLToPath } from 'node:url';
import { pathToFileURL } from 'node:url';
import source mod from './unimportable.wasm';

strictEqual(mod instanceof WebAssembly.Module, true);
const unimportableUrl = pathToFileURL(fileURLToPath(new URL('./unimportable.wasm', import.meta.url))).href;
await rejects(import(unimportableUrl));
