// Test fixture for loading WASM exports
import { strictEqual, match } from 'node:assert';
import { fileURLToPath } from 'node:url';
import { add, addImported } from './simple.wasm';
import { state } from './wasm-dep.mjs';

strictEqual(state, 'WASM Start Executed');
strictEqual(add(10, 20), 30);
strictEqual(addImported(0), 42);
strictEqual(state, 'WASM JS Function Executed');
strictEqual(addImported(1), 43);
