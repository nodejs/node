// Test WASM module with invalid import module name starting with 'wasm-js:'
import { fileURLToPath } from 'node:url';
import { pathToFileURL } from 'node:url';

const url = pathToFileURL(fileURLToPath(new URL('./invalid-import-module.wasm', import.meta.url))).href;
await import(url);
