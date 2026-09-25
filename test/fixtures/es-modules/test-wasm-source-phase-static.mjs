// Test fixture for static source phase imports
import { strictEqual } from 'node:assert';
import * as wasmExports from './wasm-source-phase.js';

strictEqual(wasmExports.mod instanceof WebAssembly.Module, true);
const AbstractModuleSourceProto = Object.getPrototypeOf(Object.getPrototypeOf(wasmExports.mod));
const toStringTag = Object.getOwnPropertyDescriptor(AbstractModuleSourceProto, Symbol.toStringTag).get;
strictEqual(toStringTag.call(wasmExports.mod), 'WebAssembly.Module');
