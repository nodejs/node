// Flags: --experimental-modules
import { expectWarning } from '../common/index.mjs';

(0, eval)('process');

expectWarning('DeprecationWarning',
              'global.process is deprecated in ECMAScript Modules. ' +
              'Please use `import process from \'process\'` instead.',
              'DEP0XXX');
