[
  "\u0000\u0000",
  "x\u0000",
  "\u0000x",
  "x\u0000x",
  " \u0000"
].forEach(idValue => {
  const encodedIdValue = encodeURIComponent(idValue);
  async_test(t => {
    const source = new EventSource("resources/last-event-id.py?idvalue=" + encodedIdValue);
    t.add_cleanup(() => source.close());
    let seenhello = false;
    source.onmessage = t.step_func(e => {
      if (e.data == "hello" && !seenhello) {
        seenhello = true;
        assert_equals(e.lastEventId, "");
      } else if(seenhello) {
        assert_equals(e.data, "hello");
        assert_equals(e.lastEventId, "");
        t.done();
      } else
        assert_unreached();
    });
  }, "EventSource: id field set to " + encodedIdValue);
});
