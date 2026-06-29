// Test fixture for js-string builtins support
import { strictEqual } from 'node:assert';
import * as wasmExports from './js-string-builtins.wasm';

strictEqual(wasmExports.getLength('hello'), 5);
strictEqual(wasmExports.concatStrings('hello', ' world'), 'hello world');
strictEqual(wasmExports.compareStrings('test', 'test'), 1);
strictEqual(wasmExports.compareStrings('test', 'different'), 0);
