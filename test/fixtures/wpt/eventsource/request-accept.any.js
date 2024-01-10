// META: title=EventSource: Accept header
      var test = async_test()
      test.step(function() {
        var source = new EventSource("resources/accept.event_stream?pipe=sub")
        source.onmessage = function(e) {
          test.step(function() {
            assert_equals(e.data, "text/event-stream")
            this.close()
          }, this)
          test.done()
        }
      })

