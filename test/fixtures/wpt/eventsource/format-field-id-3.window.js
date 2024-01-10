const ID_PERSISTS = 1,
ID_RESETS_1 = 2,
ID_RESETS_2 = 3;

async_test(testPersist, "EventSource: lastEventId persists");
async_test(testReset(ID_RESETS_1), "EventSource: lastEventId resets");
async_test(testReset(ID_RESETS_2), "EventSource: lastEventId resets (id without colon)");

function testPersist(t) {
    const source = new EventSource("resources/last-event-id2.py?type=" + ID_PERSISTS);
    let counter = 0;
    t.add_cleanup(() => source.close());
    source.onmessage = t.step_func(e => {
      counter++;
      if (counter === 1) {
          assert_equals(e.lastEventId, "1");
          assert_equals(e.data, "1");
      } else if (counter === 2) {
          assert_equals(e.lastEventId, "1");
          assert_equals(e.data, "2");
      } else if (counter === 3) {
          assert_equals(e.lastEventId, "2");
          assert_equals(e.data, "3");
      } else if (counter === 4) {
          assert_equals(e.lastEventId, "2");
          assert_equals(e.data, "4");
          t.done();
      } else {
          assert_unreached();
      }
  });
}

function testReset(type) {
  return function (t) {
    const source = new EventSource("resources/last-event-id2.py?type=" + type);
    let counter = 0;
    t.add_cleanup(() => source.close());
    source.onmessage = t.step_func(e => {
      counter++;
      if (counter === 1) {
        assert_equals(e.lastEventId, "1");
        assert_equals(e.data, "1");
      } else if (counter === 2) {
        assert_equals(e.lastEventId, "");
        assert_equals(e.data, "2");
      } else if (counter === 3) {
        assert_equals(e.lastEventId, "");
        assert_equals(e.data, "3");
        t.done();
      } else {
          assert_unreached();
      }
    });
  }
}
