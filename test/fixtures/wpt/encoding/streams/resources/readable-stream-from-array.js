'use strict';

function readableStreamFromArray(array) {
  return new ReadableStream({
    start(controller) {
      for (let entry of array) {
        controller.enqueue(entry);
      }
      controller.close();
   }
  });
}
