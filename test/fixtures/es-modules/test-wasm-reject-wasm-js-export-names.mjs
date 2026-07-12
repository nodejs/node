// Test WASM module with invalid export name starting with 'wasm-js:'
import { fileURLToPath } from 'node:url';
import { pathToFileURL } from 'node:url';

const url = pathToFileURL(fileURLToPath(new URL('./invalid-export-name-wasm-js.wasm', import.meta.url))).href;
await import(url);
