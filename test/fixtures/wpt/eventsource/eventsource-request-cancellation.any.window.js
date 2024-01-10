// META: title=EventSource: request cancellation

      var t = async_test();
      onload = t.step_func(function() {
        var url = "resources/message.py?sleep=1000&message=" + encodeURIComponent("retry:1000\ndata:abc\n\n");
        var es = new EventSource(url);
        es.onerror = t.step_func(function() {
          assert_equals(es.readyState, EventSource.CLOSED)
          t.step_timeout(function () {
            assert_equals(es.readyState, EventSource.CLOSED,
                          "After stopping the eventsource readyState should be CLOSED")
            t.done();
          }, 1000);
        });

        t.step_timeout(function() {
          window.stop()
          es.onopen = t.unreached_func("Got open event");
          es.onmessage = t.unreached_func("Got message after closing source");
        }, 0);
      });
