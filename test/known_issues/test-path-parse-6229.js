'use strict';
// Refs: https://github.com/nodejs/node/issues/6229

require('../common');
const assert = require('assert');
const path = require('path');

// The path `/foo/bar` is not the same path as `/foo/bar/`
const parsed1 = path.parse('/foo/bar');
const parsed2 = path.parse('/foo/bar/');

assert.notDeepStrictEqual(parsed1, parsed2);

// parsed1 *should* equal:
// {root: '/', dir: '/foo', base: 'bar', ext: '', name: 'bar'}
//
// parsed2 *should* equal:
// {root: '/', dir: '/foo/bar', base: '', ext: '', name: ''}
