'use strict';

if (self.importScripts) {
  self.importScripts('/resources/testharness.js');
}

test(() => {

  new CountQueuingStrategy({ highWaterMark: 4 });

}, 'Can construct a CountQueuingStrategy with a valid high water mark');

test(() => {

  for (const highWaterMark of [-Infinity, NaN, 'foo', {}, () => {}]) {
    const strategy = new CountQueuingStrategy({ highWaterMark });
    assert_equals(strategy.highWaterMark, highWaterMark, `${highWaterMark} gets set correctly`);
  }

}, 'Can construct a CountQueuingStrategy with any value as its high water mark');

test(() => {

  const highWaterMark = 1;
  const highWaterMarkObjectGetter = {
    get highWaterMark() { return highWaterMark; }
  };
  const error = new Error('wow!');
  const highWaterMarkObjectGetterThrowing = {
    get highWaterMark() { throw error; }
  };

  assert_throws({ name: 'TypeError' }, () => new CountQueuingStrategy(), 'construction fails with undefined');
  assert_throws({ name: 'TypeError' }, () => new CountQueuingStrategy(null), 'construction fails with null');
  assert_throws({ name: 'Error' }, () => new CountQueuingStrategy(highWaterMarkObjectGetterThrowing),
    'construction fails with an object with a throwing highWaterMark getter');

  // Should not fail:
  new CountQueuingStrategy('potato');
  new CountQueuingStrategy({});
  new CountQueuingStrategy(highWaterMarkObjectGetter);

}, 'CountQueuingStrategy constructor behaves as expected with strange arguments');


test(() => {

  const thisValue = null;
  const chunk = {
    get byteLength() {
      throw new TypeError('shouldn\'t be called');
    }
  };

  assert_equals(CountQueuingStrategy.prototype.size.call(thisValue, chunk), 1);

}, 'CountQueuingStrategy.prototype.size should work generically on its this and its arguments');

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

  assert_equals(CountQueuingStrategy.prototype.size(), 1, 'size returns 1 with undefined');
  assert_equals(CountQueuingStrategy.prototype.size(null), 1, 'size returns 1 with null');
  assert_equals(CountQueuingStrategy.prototype.size('potato'), 1, 'size returns 1 with non-object type');
  assert_equals(CountQueuingStrategy.prototype.size({}), 1, 'size returns 1 with empty object');
  assert_equals(CountQueuingStrategy.prototype.size(chunk), 1, 'size returns 1 with a chunk');
  assert_equals(CountQueuingStrategy.prototype.size(chunkGetter), 1, 'size returns 1 with chunk getter');
  assert_equals(CountQueuingStrategy.prototype.size(chunkGetterThrowing), 1,
    'size returns 1 with chunk getter that throws');

}, 'CountQueuingStrategy size behaves as expected with strange arguments');

test(() => {

  const strategy = new CountQueuingStrategy({ highWaterMark: 4 });

  assert_object_equals(Object.getOwnPropertyDescriptor(strategy, 'highWaterMark'),
    { value: 4, writable: true, enumerable: true, configurable: true },
    'highWaterMark property should be a data property with the value passed the constructor');
  assert_equals(typeof strategy.size, 'function');

}, 'CountQueuingStrategy instances have the correct properties');

test(() => {

  const strategy = new CountQueuingStrategy({ highWaterMark: 4 });
  assert_equals(strategy.highWaterMark, 4);

  strategy.highWaterMark = 10;
  assert_equals(strategy.highWaterMark, 10);

  strategy.highWaterMark = 'banana';
  assert_equals(strategy.highWaterMark, 'banana');

}, 'CountQueuingStrategy\'s highWaterMark property can be set to anything');

test(() => {

  assert_equals(CountQueuingStrategy.name, 'CountQueuingStrategy',
                'CountQueuingStrategy.name must be "CountQueuingStrategy"');

}, 'CountQueuingStrategy.name is correct');

done();
