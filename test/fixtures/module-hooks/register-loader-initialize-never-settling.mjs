import {register} from 'node:module';
import fixtures from '../../common/fixtures.js';

try {
  register(fixtures.fileURL('es-module-loaders/loader-initialize-never-settling.mjs'));
} catch (e) {
  console.log('caught', e.code);
}
