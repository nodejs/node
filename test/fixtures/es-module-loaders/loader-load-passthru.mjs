import { writeSync } from 'node:fs';

export async function load(url, context, next) {
  // This check is needed to make sure that we don't prevent the
  // resolution from follow-up loaders, or `'node:fs'` that we
  // use instead of `console.log`. It wouldn't be a problem
  // in real life because loaders aren't supposed to break the
  // resolution, but the ones used in our tests do, for convenience.
  if (url.includes('loader') || url === 'node:fs') {
    return next(url, context);
  }

  writeSync(1, 'load passthru\n'); // This log is deliberate
  return next(url);
}
