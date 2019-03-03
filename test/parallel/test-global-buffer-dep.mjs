// Flags: --experimental-modules
import { expectWarning } from '../common/index.mjs';

global.Buffer;

expectWarning('DeprecationWarning',
              'global.Buffer is deprecated in ECMAScript Modules. ' +
              'Please use `import { Buffer } from \'buffer\'` instead.',
              'DEP0YYY');
