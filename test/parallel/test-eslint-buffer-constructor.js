'use strict';

require('../common');

const RuleTester = require('../../tools/node_modules/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/buffer-constructor');

const message = 'Use of the Buffer() constructor has been deprecated. ' +
                'Please use either Buffer.alloc(), Buffer.allocUnsafe(), ' +
                'or Buffer.from()';

new RuleTester().run('buffer-constructor', rule, {
  valid: [
    'Buffer.from(foo)'
  ],
  invalid: [
    {
      code: 'Buffer(foo)',
      errors: [{ message }]
    },
    {
      code: 'new Buffer(foo)',
      errors: [{ message }]
    }
  ]
});
