// META: title=EventSource: empty retry field
      var test = async_test()
      test.step(function() {
        var source = new EventSource("resources/message.py?message=retry%0Adata%3Atest")
        source.onmessage = function(e) {
          test.step(function() {
            assert_equals("test", e.data)
            source.close()
          })
          test.done()
        }
      })

