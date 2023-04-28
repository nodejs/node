import '../common/index.mjs';
import fixtures from '../common/fixtures.js';
import assert from 'assert';
import { pathToFileURL } from 'url';

import(pathToFileURL(fixtures.path('/es-modules/tla/parent.mjs')))
  .then(({ default: order }) => {
    assert.deepStrictEqual(order, ['order', 'b', 'c', 'd', 'a', 'parent']);
  });
