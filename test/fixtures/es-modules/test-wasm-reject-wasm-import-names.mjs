// Test WASM module with invalid import name starting with 'wasm:'
import { fileURLToPath } from 'node:url';
import { pathToFileURL } from 'node:url';

const url = pathToFileURL(fileURLToPath(new URL('./invalid-import-name.wasm', import.meta.url))).href;
await import(url);
