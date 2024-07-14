import { writeFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { mock } from 'node:test';

writeFileSync(resolve(import.meta.dirname, './test-2.js'), 'export default 42');

try {
  if (mock.module) mock.module('Whatever, this is not significant', { namedExports: {} });
} catch {}

const { default: x } = await import('./test-2');
console.log(`Found x: ${x}`); // prints 42
