import { writeSync } from 'node:fs';


export async function load(url, context, next) {
  writeSync(1, context.foo + '\n'); // Expose actual value the hook was called with
  return next(url, context);
}
