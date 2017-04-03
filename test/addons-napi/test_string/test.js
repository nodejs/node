'use strict';
const common = require('../../common');
const assert = require('assert');

// testing api calls for string
const test_string = require(`./build/${common.buildType}/test_string`);

const str1 = 'hello world';
assert.strictEqual(test_string.Copy(str1), str1);
assert.strictEqual(test_string.Length(str1), 11);
assert.strictEqual(test_string.Utf8Length(str1), 11);

const str2 = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
assert.strictEqual(test_string.Copy(str2), str2);
assert.strictEqual(test_string.Length(str2), 62);
assert.strictEqual(test_string.Utf8Length(str2), 62);

const str3 = '?!@#$%^&*()_+-=[]{}/.,<>\'"\\';
assert.strictEqual(test_string.Copy(str3), str3);
assert.strictEqual(test_string.Length(str3), 27);
assert.strictEqual(test_string.Utf8Length(str3), 27);

const str4 = '\u{2003}\u{2101}\u{2001}';
assert.strictEqual(test_string.Copy(str4), str4);
assert.strictEqual(test_string.Length(str4), 3);
assert.strictEqual(test_string.Utf8Length(str4), 9);
