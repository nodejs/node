import { writeSync } from 'fs';

export async function resolve(specifier, context, nextResolve) {
  writeSync(1, `loader-a ${specifier}\n`);
  return nextResolve(specifier.replace(/^xxx\//, `./`));
}
