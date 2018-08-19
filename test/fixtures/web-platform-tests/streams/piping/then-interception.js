'use strict';

if (self.importScripts) {
  self.importScripts('/resources/testharness.js');
  self.importScripts('../resources/test-utils.js');
  self.importScripts('../resources/recording-streams.js');
}

function interceptThen() {
  const intercepted = [];
  const callCount = 0;
  Object.prototype.then = function(resolver) {
    if (!this.done) {
      intercepted.push(this.value);
    }
    const retval = Object.create(null);
    retval.done = ++callCount === 3;
    retval.value = callCount;
    resolver(retval);
    if (retval.done) {
      delete Object.prototype.then;
    }
  }
  return intercepted;
}

promise_test(async () => {
  const rs = new ReadableStream({
    start(controller) {
      controller.enqueue('a');
      controller.close();
    }
  });
  const ws = recordingWritableStream();

  const intercepted = interceptThen();

  await rs.pipeTo(ws);
  delete Object.prototype.then;


  assert_array_equals(intercepted, [], 'nothing should have been intercepted');
  assert_array_equals(ws.events, ['write', 'a', 'close'], 'written chunk should be "a"');
}, 'piping should not be observable');

promise_test(async () => {
  const rs = new ReadableStream({
    start(controller) {
      controller.enqueue('a');
      controller.close();
    }
  });
  const ws = recordingWritableStream();

  const [ branch1, branch2 ] = rs.tee();

  const intercepted = interceptThen();

  await branch1.pipeTo(ws);
  delete Object.prototype.then;
  branch2.cancel();

  assert_array_equals(intercepted, [], 'nothing should have been intercepted');
  assert_array_equals(ws.events, ['write', 'a', 'close'], 'written chunk should be "a"');
}, 'tee should not be observable');

done();
