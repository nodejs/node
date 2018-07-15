// Flags: --experimental-modules --loader ./test/fixtures/es-module-loaders/missing-dynamic-instantiate-hook.mjs

import {
  crashOnUnhandledRejection,
  expectsError
} from '../common';

crashOnUnhandledRejection();

import('test').catch(expectsError({
  code: 'ERR_MISSING_DYNAMIC_INSTANTIATE_HOOK',
  message: 'The ES Module loader may not return a format of \'dynamic\' ' +
    'when no dynamicInstantiate function was provided'
}));
