// Test fixture for non-identifier export names
import { strictEqual } from 'node:assert';
import * as wasmExports from './export-name-syntax-error.wasm';

strictEqual(wasmExports['?f!o:o<b>a[r]'], 12682);
