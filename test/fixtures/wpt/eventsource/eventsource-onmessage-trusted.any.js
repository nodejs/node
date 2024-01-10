// META: title=EventSource message events are trusted

"use strict";

async_test(t => {
  const source = new EventSource("resources/message.py");

  source.onmessage = t.step_func_done(e => {
    source.close();
    assert_equals(e.isTrusted, true);
  });
}, "EventSource message events are trusted");
