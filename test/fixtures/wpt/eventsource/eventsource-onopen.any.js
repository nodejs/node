// META: title=EventSource: onopen (announcing the connection)

      var test = async_test()
      test.step(function() {
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

