import { writeSync } from 'node:fs';
import { inspect } from 'node:util';

export async function resolve(specifier, context, nextResolve) {
  if (specifier.startsWith('node:')) {
    return nextResolve(specifier);
  }
  writeSync(1, `loader-a ${inspect({specifier})}\n`);
  return nextResolve(specifier.replace(/^xxx\//, `./`));
}
