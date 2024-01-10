// META: title=EventSource: Last-Event-ID (2)
    var test = async_test()
      test.step(function() {
        var source = new EventSource("resources/last-event-id.py"),
            counter = 0
        source.onmessage = function(e) {
          test.step(function() {
            if(e.data == "hello" && counter == 0) {
              counter++
              assert_equals(e.lastEventId, "…")
            } else if(counter == 1) {
              counter++
              assert_equals("…", e.data)
              assert_equals("…", e.lastEventId)
            } else if(counter == 2) {
              counter++
              assert_equals("…", e.data)
              assert_equals("…", e.lastEventId)
              source.close()
              test.done()
            } else
              assert_unreached()
          })
        }
      })
