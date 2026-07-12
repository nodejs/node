import { writeSync } from 'node:fs';
import { inspect } from 'node:util';

export async function resolve(specifier, context, nextResolve) {
  writeSync(1, `loader-b ${inspect({specifier})}\n`);
  return nextResolve(specifier.replace(/^yyy\//, `./`));
}
