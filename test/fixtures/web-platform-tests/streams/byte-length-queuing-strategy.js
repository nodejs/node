'use strict';

if (self.importScripts) {
  self.importScripts('/resources/testharness.js');
}

test(() => {

  new ByteLengthQueuingStrategy({ highWaterMark: 4 });

}, 'Can construct a ByteLengthQueuingStrategy with a valid high water mark');

test(() => {

  for (const highWaterMark of [-Infinity, NaN, 'foo', {}, () => {}]) {
    const strategy = new ByteLengthQueuingStrategy({ highWaterMark });
    assert_equals(strategy.highWaterMark, highWaterMark, `${highWaterMark} gets set correctly`);
  }

}, 'Can construct a ByteLengthQueuingStrategy with any value as its high water mark');

test(() => {

  const highWaterMark = 1;
  const highWaterMarkObjectGetter = {
    get highWaterMark() { return highWaterMark; }
  };
  const error = new Error('wow!');
  const highWaterMarkObjectGetterThrowing = {
    get highWaterMark() { throw error; }
  };

  assert_throws({ name: 'TypeError' }, () => new ByteLengthQueuingStrategy(), 'construction fails with undefined');
  assert_throws({ name: 'TypeError' }, () => new ByteLengthQueuingStrategy(null), 'construction fails with null');
  assert_throws({ name: 'Error' }, () => new ByteLengthQueuingStrategy(highWaterMarkObjectGetterThrowing),
    'construction fails with an object with a throwing highWaterMark getter');

  // Should not fail:
  new ByteLengthQueuingStrategy('potato');
  new ByteLengthQueuingStrategy({});
  new ByteLengthQueuingStrategy(highWaterMarkObjectGetter);

}, 'ByteLengthQueuingStrategy constructor behaves as expected with strange arguments');

test(() => {

  const size = 1024;
  const chunk = { byteLength: size };
  const chunkGetter = {
    get byteLength() { return size; }
  };
  const error = new Error('wow!');
  const chunkGetterThrowing = {
    get byteLength() { throw error; }
  };
  assert_throws({ name: 'TypeError' }, () => ByteLengthQueuingStrategy.prototype.size(), 'size fails with undefined');
  assert_throws({ name: 'TypeError' }, () => ByteLengthQueuingStrategy.prototype.size(null), 'size fails with null');
  assert_equals(ByteLengthQueuingStrategy.prototype.size('potato'), undefined,
    'size succeeds with undefined with a random non-object type');
  assert_equals(ByteLengthQueuingStrategy.prototype.size({}), undefined,
    'size succeeds with undefined with an object without hwm property');
  assert_equals(ByteLengthQueuingStrategy.prototype.size(chunk), size,
    'size succeeds with the right amount with an object with a hwm');
  assert_equals(ByteLengthQueuingStrategy.prototype.size(chunkGetter), size,
    'size succeeds with the right amount with an object with a hwm getter');
  assert_throws({ name: 'Error' }, () => ByteLengthQueuingStrategy.prototype.size(chunkGetterThrowing),
    'size fails with the error thrown by the getter');

}, 'ByteLengthQueuingStrategy size behaves as expected with strange arguments');

test(() => {

  const thisValue = null;
  const returnValue = { 'returned from': 'byteLength getter' };
  const chunk = {
    get byteLength() { return returnValue; }
  };

  assert_equals(ByteLengthQueuingStrategy.prototype.size.call(thisValue, chunk), returnValue);

}, 'ByteLengthQueuingStrategy.prototype.size should work generically on its this and its arguments');

test(() => {

  const strategy = new ByteLengthQueuingStrategy({ highWaterMark: 4 });

  assert_object_equals(Object.getOwnPropertyDescriptor(strategy, 'highWaterMark'),
    { value: 4, writable: true, enumerable: true, configurable: true },
    'highWaterMark property should be a data property with the value passed the constructor');
  assert_equals(typeof strategy.size, 'function');

}, 'ByteLengthQueuingStrategy instances have the correct properties');

test(() => {

  const strategy = new ByteLengthQueuingStrategy({ highWaterMark: 4 });
  assert_equals(strategy.highWaterMark, 4);

  strategy.highWaterMark = 10;
  assert_equals(strategy.highWaterMark, 10);

  strategy.highWaterMark = 'banana';
  assert_equals(strategy.highWaterMark, 'banana');

}, 'ByteLengthQueuingStrategy\'s highWaterMark property can be set to anything');

test(() => {

  assert_equals(ByteLengthQueuingStrategy.name, 'ByteLengthQueuingStrategy',
                'ByteLengthQueuingStrategy.name must be "ByteLengthQueuingStrategy"');

}, 'ByteLengthQueuingStrategy.name is correct');

done();
