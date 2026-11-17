import { registerHooks } from 'node:module';
import type { ResolveHookSync } from 'node:module';

// Pass through
const resolve: ResolveHookSync = function resolve(specifier, context, nextResolve) {
  return nextResolve(specifier, context);
};

registerHooks({ resolve });
