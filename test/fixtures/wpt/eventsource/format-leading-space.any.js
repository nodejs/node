// META: title=EventSource: leading space
      var test = async_test()
      test.step(function() {
        var source = new EventSource("resources/message.py?message=data%3A%09test%0Ddata%3A%20%0Adata%3Atest")
        source.onmessage = function(e) {
          test.step(function() {
            assert_equals("\ttest\n\ntest", e.data)
            source.close()
          })
          test.done()
        }
      })
    // also used a CR as newline once

