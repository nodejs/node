// Test fixture for dynamic source phase imports
import { strictEqual } from 'node:assert';
import * as wasmExports from './wasm-source-phase.js';

strictEqual(wasmExports.mod instanceof WebAssembly.Module, true);
strictEqual(await wasmExports.dyn('./simple.wasm') instanceof WebAssembly.Module, true);
const AbstractModuleSourceProto = Object.getPrototypeOf(Object.getPrototypeOf(wasmExports.mod));
const toStringTag = Object.getOwnPropertyDescriptor(AbstractModuleSourceProto, Symbol.toStringTag).get;
strictEqual(toStringTag.call(wasmExports.mod), 'WebAssembly.Module');
