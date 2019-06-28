'use strict';

require('../common');
const { deepStrictEqual } = require('assert');

deepStrictEqual(
  require('fs').promises,
  require('fs/promises'),
);

deepStrictEqual(
  require('dns').promises,
  require('dns/promises'),
);
