// META: title=EventSource: onmessage

      var test = async_test()
      test.step(function() {
        var source = new EventSource("resources/message.py")
        source.onmessage = function(e) {
          test.step(function() {
            assert_equals("data", e.data)
            source.close()
          })
          test.done()
        }
      })

