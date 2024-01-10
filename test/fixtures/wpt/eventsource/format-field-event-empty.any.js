// META: title=EventSource: empty "event" field
      var test = async_test()
      test.step(function() {
        var source = new EventSource("resources/message.py?message=event%3A%20%0Adata%3Adata")
        source.onmessage = function(e) {
          test.step(function() {
            assert_equals("data", e.data)
            this.close()
          }, this)
          test.done()
        }
      })

