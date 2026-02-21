import source mod from './simple.wasm';

export function dyn (specifier) {
  return import.source(specifier);
}

export { mod };
