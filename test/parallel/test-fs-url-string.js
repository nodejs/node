// Flags: --fs-url-strings
'use strict';

require('../common');
const assert = require('assert');
const { pathToFileURL } = require('url');
const { readFileSync } = require('fs');

const url = pathToFileURL(__filename);

const data1 = readFileSync(url.toString());
const data2 = readFileSync(url);

assert.deepStrictEqual(data1, data2);
