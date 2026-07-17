// Test fixture that dynamic source phase imports don't execute
import { fileURLToPath } from 'node:url';
import { pathToFileURL } from 'node:url';

const unimportableUrl = pathToFileURL(fileURLToPath(new URL('./unimportable.wasm', import.meta.url))).href;
await import.source(unimportableUrl);
