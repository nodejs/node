'use strict';
const common = require('../../common');
const assert = require('assert');

// testing api calls for string
const test_string = require(`./build/${common.buildType}/test_string`);

const empty = '';
assert.strictEqual(test_string.TestLatin1(empty), empty);
assert.strictEqual(test_string.TestUtf8(empty), empty);
assert.strictEqual(test_string.TestUtf16(empty), empty);
assert.strictEqual(test_string.Length(empty), 0);
assert.strictEqual(test_string.Utf8Length(empty), 0);

const str1 = 'hello world';
assert.strictEqual(test_string.TestLatin1(str1), str1);
assert.strictEqual(test_string.TestUtf8(str1), str1);
assert.strictEqual(test_string.TestUtf16(str1), str1);
assert.strictEqual(test_string.TestLatin1Insufficient(str1), str1.slice(0, 3));
assert.strictEqual(test_string.TestUtf8Insufficient(str1), str1.slice(0, 3));
assert.strictEqual(test_string.TestUtf16Insufficient(str1), str1.slice(0, 3));
assert.strictEqual(test_string.Length(str1), 11);
assert.strictEqual(test_string.Utf8Length(str1), 11);

const str2 = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
assert.strictEqual(test_string.TestLatin1(str2), str2);
assert.strictEqual(test_string.TestUtf8(str2), str2);
assert.strictEqual(test_string.TestUtf16(str2), str2);
assert.strictEqual(test_string.TestLatin1Insufficient(str2), str2.slice(0, 3));
assert.strictEqual(test_string.TestUtf8Insufficient(str2), str2.slice(0, 3));
assert.strictEqual(test_string.TestUtf16Insufficient(str2), str2.slice(0, 3));
assert.strictEqual(test_string.Length(str2), 62);
assert.strictEqual(test_string.Utf8Length(str2), 62);

const str3 = '?!@#$%^&*()_+-=[]{}/.,<>\'"\\';
assert.strictEqual(test_string.TestLatin1(str3), str3);
assert.strictEqual(test_string.TestUtf8(str3), str3);
assert.strictEqual(test_string.TestUtf16(str3), str3);
assert.strictEqual(test_string.TestLatin1Insufficient(str3), str3.slice(0, 3));
assert.strictEqual(test_string.TestUtf8Insufficient(str3), str3.slice(0, 3));
assert.strictEqual(test_string.TestUtf16Insufficient(str3), str3.slice(0, 3));
assert.strictEqual(test_string.Length(str3), 27);
assert.strictEqual(test_string.Utf8Length(str3), 27);

const str4 = '\u{2003}\u{2101}\u{2001}\u{202}\u{2011}';
assert.strictEqual(test_string.TestUtf8(str4), str4);
assert.strictEqual(test_string.TestUtf16(str4), str4);
assert.strictEqual(test_string.TestUtf8Insufficient(str4), str4.slice(0, 1));
assert.strictEqual(test_string.TestUtf16Insufficient(str4), str4.slice(0, 3));
assert.strictEqual(test_string.Length(str4), 5);
assert.strictEqual(test_string.Utf8Length(str4), 14);
