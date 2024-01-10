// META: title=EventSource: reconnection

      function doReconn(url, title) {
        var test = async_test(document.title + " " + title)
        test.step(function() {
          var source = new EventSource(url)
          source.onmessage = test.step_func(function(e) {
            assert_equals(e.data, "data")
            source.close()
            test.done()
          })
        })
      }

      doReconn("resources/status-reconnect.py?status=200",
        "200")


      var t = async_test(document.title + ", test reconnection events");
      t.step(function() {
        var opened = false, reconnected = false,
            source = new EventSource("resources/status-reconnect.py?status=200&ok_first&id=2");

        source.onerror = t.step_func(function(e) {
          assert_equals(e.type, 'error');
          assert_equals(source.readyState, source.CONNECTING, "readyState");
          assert_true(opened, "connection is opened earlier");

          reconnected = true;
        });

        source.onmessage = t.step_func(function(e) {
          if (!opened) {
            opened = true;
            assert_false(reconnected, "have reconnected before first message");
            assert_equals(e.data, "ok");
          }
          else {
            assert_true(reconnected, "Got reconnection event");
            assert_equals(e.data, "data");
            source.close()
            t.done()
          }
        });
      });


