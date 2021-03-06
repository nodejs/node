// Flags: --no-harmony-top-level-await

import {
  mustCall,
  disableCrashOnUnhandledRejection
} from '../common/index.mjs';

disableCrashOnUnhandledRejection();

process.on('unhandledRejection', mustCall());
Promise.reject(new Error('should not be fatal error'));
