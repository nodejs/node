'use strict';
const fixtures = require('../../../common/fixtures');
const { spec } = require('node:test/reporters');
const { run } = require('node:test');

const files = [
  fixtures.path('test-runner', 'bail', 'bail-test-1-pass.js'),
  fixtures.path('test-runner', 'bail', 'bail-test-2-fail.js'),
  fixtures.path('test-runner', 'bail', 'bail-test-3-pass.js'),
  fixtures.path('test-runner', 'bail', 'bail-test-4-pass.js'),
];

run({ bail: true, concurrency: 1, isolation: 'none', files }).compose(spec).compose(process.stdout);
