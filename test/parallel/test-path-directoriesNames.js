'use strict';
require('../common');
const assert = require('assert');
const path = require('path');

// A windows path
assert.strictEqual(
  path.directoriesNames(
    'D:\\DDL\\ANIME\\les chevaliers du zodiaques\\whateverFile.avi'
  ),
  ['D:', 'DDL', 'ANIME', 'les chevaliers du zodiaques']
);

// A Linux path
assert.strictEqual(
  path.directoriesNames(
    'D:/DDL/EBOOK/whatEverFile.pub'
  ),
  [ 'D:', 'DDL', 'EBOOK' ]
);

// Path near root (Windows / Linux)
assert.strictEqual(
  path.directoriesNames(
    '/foo'
  ),
  ['/']
);

assert.strictEqual(
  path.directoriesNames(
    'D:\\test.avi'
  ),
  ['D:']
);
