import { mock } from 'node:test';

try {
  if (mock.module) mock.module('Whatever, this is not significant', { namedExports: {} });
} catch {}

const { string } = await import('./basic-esm');
console.log(`Found string: ${string}`); // prints 'original esm string'
