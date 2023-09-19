'use strict';
const test = require('node:test');



test('level 0a', { concurrency: 4 }, async (t) => {
  t.test('level 1a', async (t) => {
  });

  t.test('level 1b', async (t) => {
    throw new Error('level 1b error');
  });

  t.test('level 1c', { skip: 'aaa' }, async (t) => {
  });

  t.test('level 1d', async (t) => {
    t.diagnostic('level 1d diagnostic');
  });
});

test('level 0b', async (t) => {
  throw new Error('level 0b error');
});