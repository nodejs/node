// META: title=EventSource: constructor (act as if there is a network error)

      function fetchFail(url) {
        var test = async_test(document.title + " (" + url + ")")
        test.step(function() {
          var source = new EventSource(url)
          source.onerror = function(e) {
            test.step(function() {
              assert_equals(source.readyState, source.CLOSED)
              assert_false(e.hasOwnProperty('data'))
            })
            test.done()
          }
        })
      }
      fetchFail("ftp://example.not/")
      fetchFail("about:blank")
      fetchFail("mailto:whatwg@awesome.example")
      fetchFail("javascript:alert('FAIL')")
    // This tests "fails the connection" as well as making sure a simple
    //      event is dispatched and not a MessageEvent
