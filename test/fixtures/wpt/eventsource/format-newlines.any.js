// META: title=EventSource: newline fest
      var test = async_test()
      test.step(function() {
        var source = new EventSource("resources/message.py?message=data%3Atest%0D%0Adata%0Adata%3Atest%0D%0A%0D&newline=none")
        source.onmessage = function(e) {
          test.step(function() {
            assert_equals("test\n\ntest", e.data)
            source.close()
          })
          test.done()
        }
      })

