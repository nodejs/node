import '../common/index.mjs';
import { strictEqual } from 'assert';

import asdf from
  '../fixtures/es-modules/package-type-module/nested-default-type/module.js';

strictEqual(asdf, 'asdf');
