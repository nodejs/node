import { writeSync } from 'fs';

export async function load(url, context, next) {
  writeSync(1, `${context.foo}\n`); // This log is deliberate
  return next(url, context);
}
