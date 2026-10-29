// Test fixture for properly escaping import names
import { strictEqual } from 'node:assert';
import * as wasmExports from './import-name.wasm';

strictEqual(wasmExports.xor(), 12345);
