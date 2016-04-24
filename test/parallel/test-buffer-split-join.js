'use strict';
const assert = require('assert');
const common = require('../common');

assert(common);

/*
  gatekeepers
 */
assert.throws(() => {
  const buf = new Buffer('Hello World, the day of tomorrows past');
  buf.split('o');
}, TypeError);

assert.doesNotThrow(() => {
  const buf = new Buffer('Hello World, the day of tomorrows past');
  buf.split(new Buffer('o'));
}, TypeError);

assert.throws(() => {
  Buffer.join(['String']);
}, TypeError);

assert.doesNotThrow(() => {
  const buf = new Buffer('Buffer from String');
  Buffer.join([buf]);
}, TypeError);

assert.doesNotThrow(() => {
  Buffer.join([new Buffer('Buffer from String')]);
}, Error);

/*
  e2e
 */
{
  const buf = new Buffer('Hello World, the day of tomorrows past');
  const res = buf.split(new Buffer('o'));

  assert(res.length === 7);

  assert(Buffer.compare(res[0], new Buffer('Hell')) === 0);
  assert(Buffer.compare(res[1], new Buffer(' W')) === 0);
  assert(Buffer.compare(res[2], new Buffer('rld, the day ')) === 0);
  assert(Buffer.compare(res[3], new Buffer('f t')) === 0);
  assert(Buffer.compare(res[4], new Buffer('m')) === 0);
  assert(Buffer.compare(res[5], new Buffer('rr')) === 0);
  assert(Buffer.compare(res[6], new Buffer('ws past')) === 0);
}

/*
  edge cases
 */
{
  // Returns empty buffers for edges
  const buf = new Buffer('ABCdEF');
  const res_1 = buf.split(new Buffer('F'));
  const res_2 = buf.split(new Buffer('A'));

  assert(res_1.length === 2);
  assert(res_2.length === 2);

  assert(Buffer.compare(res_1[0], new Buffer('ABCdE')) === 0);
  assert(Buffer.compare(res_1[1], new Buffer('')) === 0);
  assert(Buffer.compare(res_2[1], new Buffer('BCdEF')) === 0);
  assert(Buffer.compare(res_2[0], new Buffer('')) === 0);
}

{
  // Returns array of == buffer.length when sep buffer is empty
  const buf = new Buffer('ABCdEF');
  const res = buf.split(new Buffer(''));
  assert(res.length === 6);

  assert(Buffer.compare(res[0], new Buffer('A')) === 0);
  assert(Buffer.compare(res[1], new Buffer('B')) === 0);
  assert(Buffer.compare(res[2], new Buffer('C')) === 0);
  assert(Buffer.compare(res[3], new Buffer('d')) === 0);
  assert(Buffer.compare(res[4], new Buffer('E')) === 0);
  assert(Buffer.compare(res[5], new Buffer('F')) === 0);
}

{
  // Returns array of with the the buffer
  const buf = new Buffer('ABCdEF');
  const res = buf.split();
  assert(res.length === 1);

  assert(Buffer.compare(res[0], new Buffer('ABCdEF')) === 0);
}

{
  const res = Buffer.join([
    new Buffer('Hello'),
    new Buffer(' World')
  ]);
  assert(Buffer.compare(res, new Buffer('Hello World')) === 0);
}

{
  const res = Buffer.join([
    new Buffer('Hello'),
    new Buffer('World'),
    new Buffer(','),
    new Buffer('the day of tomorrows past')
  ], new Buffer('\n'));
  assert(Buffer.compare(res, new Buffer('Hello\nWorld\n,\nthe day of ' +
                                                      'tomorrows past')) === 0);
}

assert.throws(() => {
  Buffer.join([
    new Buffer('Hello'),
    ' ',
    new Buffer('World')
  ]);
}, TypeError);

{
  const res = Buffer.join([
    new Buffer('Hello')
  ]);
  assert(Buffer.compare(res, new Buffer('Hello')) === 0);
}

{
  const res = Buffer.join([
    new Buffer('Hello')
  ], new Buffer('\n'));
  assert(Buffer.compare(res, new Buffer('Hello')) === 0);
}

assert.throws(() => {
  Buffer.join([]);
}, TypeError);

/*
  Non-string buffers
 */
{
  const res = Buffer.from([1, 2, 3, 4, 5, 6, 10, 100]).split(new Buffer([10]));
  assert(res.length === 2);
  assert(Buffer.compare(res[0], new Buffer([1, 2, 3, 4, 5, 6])) === 0);
  assert(Buffer.compare(res[1], new Buffer([100])) === 0);

  const res_2 = Buffer.join(res, new Buffer([11]));
  assert(Buffer.compare(res_2, new Buffer([1, 2, 3, 4, 5, 6, 11, 100])) === 0);
}

{
  const buf = Buffer.alloc(11, 'aGVsbG8gd29ybGQ=', 'base64'); // 'hello world'
  const buf_2 = Buffer.alloc(1, 'IA==', 'base64')     ;    // ' '
  const res_1 = buf.split(buf_2);                          // ['hello', 'world']
  assert(Buffer.compare(res_1[0], new Buffer('hello')) === 0);
  assert(Buffer.compare(res_1[1], new Buffer('world')) === 0);

  const res_2 = Buffer.join(res_1, Buffer.alloc(1, 'Cg==', 'base64')); // '\n'
  assert(Buffer.compare(res_2, new Buffer('hello\nworld')) === 0);
}
