// META: title=EventSource: document.domain

      var test = async_test()
      test.step(function() {
        document.domain = document.domain
        source = new EventSource("resources/message.py")
        source.onopen = function(e) {
          test.step(function() {
            assert_equals(source.readyState, source.OPEN)
            assert_false(e.hasOwnProperty('data'))
            assert_false(e.bubbles)
            assert_false(e.cancelable)
            this.close()
          }, this)
          test.done()
        }
      })
    // Apart from document.domain equivalent to the onopen test.
