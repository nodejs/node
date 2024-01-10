// META: title=EventSource: BOM

      var test = async_test(),
          hasbeenone = false,
          hasbeentwo = false
      test.step(function() {
        var source = new EventSource("resources/message.py?message=%EF%BB%BFdata%3A1%0A%0A%EF%BB%BFdata%3A2%0A%0Adata%3A3")
        source.addEventListener("message", listener, false)
      })
      function listener(e) {
        test.step(function() {
          if(e.data == "1")
            hasbeenone = true
          if(e.data == "2")
            hasbeentwo = true
          if(e.data == "3") {
            assert_true(hasbeenone)
            assert_false(hasbeentwo)
            this.close()
            test.done()
          }
        }, this)
      }

