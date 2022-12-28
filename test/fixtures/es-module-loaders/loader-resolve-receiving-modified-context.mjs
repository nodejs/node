import { writeSync } from 'fs';

export async function resolve(specifier, context, next) {
  writeSync(1, `${context.foo}\n`); // This log is deliberate
  return next(specifier, context);
}
