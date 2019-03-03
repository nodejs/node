// Flags: --experimental-modules
import { expectWarning } from '../common/index.mjs';

process;

expectWarning('DeprecationWarning',
              'global.process is deprecated in ECMAScript Modules. ' +
              'Please use `import process from \'process\'` instead.',
              'DEP0XXX');
