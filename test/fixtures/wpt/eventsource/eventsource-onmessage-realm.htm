<!DOCTYPE html>
<meta charset="utf-8">
<title>EventSource: message event Realm</title>
<link rel="help" href="https://html.spec.whatwg.org/multipage/comms.html#dispatchMessage">
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>

<iframe src="resources/eventsource-onmessage-realm.htm"></iframe>

<script>
"use strict";

async_test(t => {
  window.onload = t.step_func(() => {
    const source = new frames[0].EventSource("message.py");
    t.add_cleanup(() => {
      source.close();
    });

    source.onmessage = t.step_func_done(e => {
      assert_equals(e.constructor, frames[0].MessageEvent);
    });
  });
}, "the MessageEvent must be created in the Realm of the EventSource");
</script>
