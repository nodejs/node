import resolveAsFS from './dep.js';
import fs from 'fs';

let correct = resolveAsFS === fs && typeof resolveAsFS === 'object';
process.exit(correct ? 0 : 1);
