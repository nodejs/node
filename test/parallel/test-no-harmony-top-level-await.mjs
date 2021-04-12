// Flags: --no-harmony-top-level-await

import {
  mustCall,
} from '../common/index.mjs';

process.on('unhandledRejection', mustCall());
Promise.reject(new Error('should not be fatal error'));
