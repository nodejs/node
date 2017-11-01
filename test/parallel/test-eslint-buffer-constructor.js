'use strict';

require('../common');

const RuleTester = require('../../tools/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/buffer-constructor');

const message = 'Use of the Buffer() constructor has been deprecated. ' +
                'Please use either Buffer.alloc(), Buffer.allocUnsafe(), ' +
                'or Buffer.from()';

new RuleTester().run('buffer-constructor', rule, {
  valid: [
    'Buffer.from(foo)',
    'Buffer.from(foo, bar)',
    'Buffer.alloc(foo)',
    'Buffer.alloc(foo, bar)',
    'Buffer.allocUnsafe(foo)',
    'Buffer.allocUnsafe(foo, bar)'
  ],
  invalid: [
    {
      code: 'Buffer(foo)',
      errors: [{ message }]
    },
    {
      code: 'Buffer(foo, bar)',
      errors: [{ message }]
    },
    {
      code: 'new Buffer(foo)',
      errors: [{ message }]
    },
    {
      code: 'new Buffer(foo, bar)',
      errors: [{ message }]
    }
  ]
});
