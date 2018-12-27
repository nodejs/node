import { expectsError, requireFlags } from '../common';

const flag =
  '--loader=' +
  './test/fixtures/es-module-loaders/missing-dynamic-instantiate-hook.mjs';

if (!process.execArgv.includes(flag)) {
  // Include `--experimental-modules` explicitly for workers.
  requireFlags(['--experimental-modules', flag]);
}

import('test').catch(expectsError({
  code: 'ERR_MISSING_DYNAMIC_INSTANTIATE_HOOK',
  message: 'The ES Module loader may not return a format of \'dynamic\' ' +
    'when no dynamicInstantiate function was provided'
}));
