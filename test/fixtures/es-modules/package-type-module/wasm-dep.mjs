import { strictEqual } from 'assert';

export function jsFn () {
  state = 'WASM JS Function Executed';
  return 42;
}

export let state = 'JS Function Executed';

export function jsInitFn () {
  strictEqual(state, 'JS Function Executed');
  state = 'WASM Start Executed';
}

console.log('executed');
