import { writeSync } from 'node:fs';


export async function resolve(specifier, context, next) {
  // This check is needed to make sure that we don't prevent the
  // resolution from follow-up loaders. It wouldn't be a problem
  // in real life because loaders aren't supposed to break the
  // resolution, but the ones used in our tests do, for convenience.
  if (specifier === 'node:fs' || specifier.includes('loader')) {
    return next(specifier);
  }

  writeSync(1, 'resolve 42' + '\n'); // Signal that this specific hook ran
  writeSync(1, `next<HookName>: ${next.name}\n`); // Expose actual value the hook was called with

  return next('file:///42.mjs');
}
