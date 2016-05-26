'use strict';
var common = require('../common');
var assert = require('assert');
var path = require('path');
var fs = require('fs');

var fixture = path.join(common.fixturesDir, 'x.txt');

assert.equal('xyz\n', fs.readFileSync(fixture));
