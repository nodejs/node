// META: title=EventSource: Cache-Control
      var crossdomain = location.href
                    .replace('://', '://www2.')
                    .replace(/\/[^\/]*$/, '/')

      // running it twice to check whether it stays consistent
      function cacheTest(url) {
        var test = async_test(url + "1")
        // Recursive test. This avoids test that timeout
        var test2 = async_test(url + "2")
        test.step(function() {
          var source = new EventSource(url)
          source.onmessage = function(e) {
            test.step(function() {
              assert_equals(e.data, "no-cache")
              this.close()
              test2.step(function() {
                var source2 = new EventSource(url)
                source2.onmessage = function(e) {
                  test2.step(function() {
                    assert_equals(e.data, "no-cache")
                    this.close()
                  }, this)
                  test2.done()
                }
              })
            }, this)
            test.done()
          }
        })
      }

      cacheTest("resources/cache-control.event_stream?pipe=sub")
      cacheTest(crossdomain + "resources/cors.py?run=cache-control")

