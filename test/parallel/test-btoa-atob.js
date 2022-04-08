'use strict';

require('../common');

const { strictEqual, throws } = require('assert');
const buffer = require('buffer');

// Exported on the global object
strictEqual(globalThis.atob, buffer.atob);
strictEqual(globalThis.btoa, buffer.btoa);

// Throws type error on no argument passed
throws(() => buffer.atob(), /TypeError/);
throws(() => buffer.btoa(), /TypeError/);

strictEqual(atob(' '), '');
strictEqual(atob('  YW\tJ\njZA=\r= '), 'abcd');

throws(() => buffer.atob(undefined), /ValidationError/);
throws(() => buffer.atob(false), /ValidationError/);
throws(() => buffer.atob(1), /ValidationError/);
throws(() => buffer.atob(0), /ValidationError/);
throws(() => buffer.atob('a'), /ValidationError/);
throws(() => buffer.atob('a '), /ValidationError/);
throws(() => buffer.atob(' a'), /ValidationError/);
throws(() => buffer.atob('aaaaa'), /ValidationError/);
