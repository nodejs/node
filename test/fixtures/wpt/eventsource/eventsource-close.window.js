// META: title=EventSource: close()

      var test = async_test()
      test.step(function() {
        var source = new EventSource("resources/message.py")
        assert_equals(source.readyState, source.CONNECTING, "connecting readyState");
        source.onopen = this.step_func(function() {
          assert_equals(source.readyState, source.OPEN, "open readyState");
          source.close()
          assert_equals(source.readyState, source.CLOSED, "closed readyState");
          this.done()
        })
      })

      var test2 = async_test(document.title + ", test events");
      test2.step(function() {
        var count = 0, reconnected = false,
            source = new EventSource("resources/reconnect-fail.py?id=" + new Date().getTime());

        source.onerror = this.step_func(function(e) {
          assert_equals(e.type, 'error');
          switch(count) {
            // reconnecting after first message
            case 1:
              assert_equals(source.readyState, source.CONNECTING, "reconnecting readyState");

              reconnected = true;
              break;

            // one more reconnect to get to the closing
            case 2:
              assert_equals(source.readyState, source.CONNECTING, "last reconnecting readyState");
              count++;
              break;

            // close
            case 3:
              assert_equals(source.readyState, source.CLOSED, "closed readyState");

              // give some time for errors to hit us
              test2.step_timeout(function() { this.done(); }, 100);
              break;

            default:
              assert_unreached("Error handler with msg count " + count);
          }

        });

        source.onmessage = this.step_func(function(e) {
          switch(count) {
            case 0:
              assert_true(!reconnected, "no error event run");
              assert_equals(e.data, "opened", "data");
              break;

            case 1:
              assert_true(reconnected, "have reconnected");
              assert_equals(e.data, "reconnected", "data");
              break;

            default:
              assert_unreached("Dunno what to do with message number " + count);
          }

          count++;
        });

      });

