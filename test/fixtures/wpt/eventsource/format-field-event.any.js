// META: title=EventSource: custom event name
      var test = async_test(),
          dispatchedtest = false
      test.step(function() {
        var source = new EventSource("resources/message.py?message=event%3Atest%0Adata%3Ax%0A%0Adata%3Ax")
        source.addEventListener("test", function() { test.step(function() { dispatchedtest = true }) }, false)
        source.onmessage = function() {
          test.step(function() {
            assert_true(dispatchedtest)
            this.close()
          }, this)
          test.done()
        }
      })

