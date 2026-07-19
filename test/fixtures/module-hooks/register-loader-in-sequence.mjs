import {register} from 'node:module';
import fixtures from '../../common/fixtures.js';

console.log('result 1', register(
  fixtures.fileURL('es-module-loaders/hooks-initialize.mjs')
));
console.log('result 2', register(
  fixtures.fileURL('es-module-loaders/hooks-initialize.mjs')
));

await import('node:os');
