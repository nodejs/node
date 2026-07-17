'use strict';

function readableStreamToArray(stream) {
  var array = [];
  var writable = new WritableStream({
    write(chunk) {
      array.push(chunk);
    }
  });
  return stream.pipeTo(writable).then(() => array);
}
