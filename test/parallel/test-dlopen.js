'use strict';
var common = require('../common');
var assert = require('assert');
var path = require('path');
var endianness = require('os').endianness();

var dl = require('dlopen');

var root = path.join(__dirname, '..', '..');
var libPath = path.join(root, 'out', 'Release', 'libtest' + dl.extension);
console.log(libPath);

var libtest = dl.dlopen(libPath);
console.log(libtest);

// EXPORT int six = 6
var sixSymPtr = dl.dlsym(libtest, 'six');
// TODO: use `sizeof.int`
var sixSym = sixSymPtr['readPointer' + endianness](0, 4);
assert.equal(6, sixSym['readInt' + (4 * 8) + endianness](0));

// EXPORT void* n = NULL;
var nSymPtr = dl.dlsym(libtest, 'n');
// TODO: use `sizeof.pointer`
var nSym = nSymPtr['readPointer' + endianness](0, 8);
assert.strictEqual(null, nSym['readPointer' + endianness](0));

// EXPORT char str[] = "hello world";
var strSymPtr = dl.dlsym(libtest, 'str');
// XXX: We need a way to read a null-terminated array :( :( :(
var strSym = strSymPtr['readPointer' + endianness](0, 12);
assert.equal('hello world', strSym.toString('ascii', 0, 11));
assert.equal(0, strSym[11]);

// EXPORT uint64_t factorial(int max)
var factorialSymPtr = dl.dlsym(libtest, 'factorial');
// TODO: use `sizeof.pointer`
var factorialSym = factorialSymPtr['readPointer' + endianness](0, 0);

// EXPORT intptr_t factorial_addr = (intptr_t)factorial;
var factorialAddrSymPtr = dl.dlsym(libtest, 'factorial_addr');
var factorialAddrSym = factorialAddrSymPtr['readPointer' + endianness](0, 8);
var factorialSym2 = factorialAddrSym['readPointer' + endianness](0, 0);

assert.equal(factorialSym.address(), factorialSym2.address());


// we're done ☺
dl.dlclose(libtest);
