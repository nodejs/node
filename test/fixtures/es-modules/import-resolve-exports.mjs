import { strictEqual } from 'assert';

const resolved = import.meta.resolve('pkgexports-sugar');
strictEqual(typeof resolved, 'string');
