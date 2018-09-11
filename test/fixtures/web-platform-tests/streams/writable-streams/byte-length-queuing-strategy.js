'use strict';

if (self.importScripts) {
  self.importScripts('/resources/testharness.js');
}

promise_test(() => {
  let isDone = false;
  const ws = new WritableStream(
    {
      write() {
        return new Promise(resolve => {
          setTimeout(() => {
            isDone = true;
            resolve();
          }, 200);
        });
      },

      close() {
        assert_true(isDone, 'close is only called once the promise has been resolved');
      }
    },
    new ByteLengthQueuingStrategy({ highWaterMark: 1024 * 16 })
  );

  const writer = ws.getWriter();
  writer.write({ byteLength: 1024 });

  return writer.close();
}, 'Closing a writable stream with in-flight writes below the high water mark delays the close call properly');

done();
