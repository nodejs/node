// Flags: --experimental-modules --experimental-wasm-modules
import '../common/index.mjs';
import { add, addImported } from '../fixtures/es-modules/simple.wasm';
import { state } from '../fixtures/es-modules/wasm-dep.mjs';
import { strictEqual } from 'assert';

strictEqual(state, 'WASM Start Executed');

strictEqual(add(10, 20), 30);

strictEqual(addImported(0), 42);

strictEqual(state, 'WASM JS Function Executed');

strictEqual(addImported(1), 43);
