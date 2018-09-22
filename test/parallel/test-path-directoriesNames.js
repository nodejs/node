'use strict';
require('../common');
const assert = require('assert');
const path = require('path');

// A windows path
assert.strictEqual(
  JSON.stringify(
    path.directoriesNames(
      'D:\\DDL\\ANIME\\les chevaliers du zodiaques\\whateverFile.avi'
    )
  ),
  JSON.stringify(['D:', 'DDL', 'ANIME', 'les chevaliers du zodiaques'])
);

// A Linux path
assert.strictEqual(
  JSON.stringify(path.directoriesNames('D:/DDL/EBOOK/whatEverFile.pub')),
  JSON.stringify(['D:', 'DDL', 'EBOOK'])
);

// Path near root (Windows / Linux)
assert.strictEqual(
  JSON.stringify(path.directoriesNames('/foo')),
  JSON.stringify(['/'])
);

assert.strictEqual(
  JSON.stringify(path.directoriesNames('D:\\test.avi')),
  JSON.stringify(['D:'])
);
