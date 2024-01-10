// META: title=EventSource: data field parsing

      var test = async_test()
      test.step(function() {
        var source = new EventSource("resources/message.py?message=data%3A%0A%0Adata%0Adata%0A%0Adata%3Atest"),
        counter = 0
        source.onmessage = function(e) {
          test.step(function() {
            if(counter == 0) {
              assert_equals("", e.data)
            } else if(counter == 1) {
              assert_equals("\n", e.data)
            } else if(counter == 2) {
              assert_equals("test", e.data)
              source.close()
              test.done()
            } else {
              assert_unreached()
            }
            counter++
          })
        }
      })
