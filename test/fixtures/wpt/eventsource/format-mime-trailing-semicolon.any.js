// META: title=EventSource: MIME type with trailing ;
      var test = async_test()
      test.step(function() {
        var source = new EventSource("resources/message.py?mime=text/event-stream%3B")
        source.onopen = function() {
          test.step(function() {
            assert_equals(source.readyState, source.OPEN)
            source.close()
          })
          test.done()
        }
        source.onerror = function() {
          test.step(function() {
            assert_unreached()
            source.close()
          })
          test.done()
        }
      })

