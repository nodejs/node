import {register} from 'node:module';
import fixtures from '../../common/fixtures.js';

register(
  fixtures.fileURL('es-module-loaders/loader-load-foo-or-42.mjs'),
  new URL('data:'),
);

import('node:os').then((result) => {
  console.log(JSON.stringify(result));
});
