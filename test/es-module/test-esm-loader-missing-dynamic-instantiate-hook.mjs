import { expectsError, requireFlags } from '../common';

const longFlag =
  '--loader=' +
  './test/fixtures/es-module-loaders/missing-dynamic-instantiate-hook.mjs';

requireFlags('--experimental-modules', longFlag);

import('test').catch(expectsError({
  code: 'ERR_MISSING_DYNAMIC_INSTANTIATE_HOOK',
  message: 'The ES Module loader may not return a format of \'dynamic\' ' +
    'when no dynamicInstantiate function was provided'
}));
