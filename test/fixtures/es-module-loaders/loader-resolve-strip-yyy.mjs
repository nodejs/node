import { writeSync } from 'node:fs';

export async function resolve(specifier, context, nextResolve) {
  writeSync(1, `loader-b ${specifier}\n`);
  return nextResolve(specifier.replace(/^yyy\//, `./`));
}
