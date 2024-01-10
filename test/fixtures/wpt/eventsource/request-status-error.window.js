// META: title=EventSource: incorrect HTTP status code
      function statusTest(status) {
        var test = async_test(document.title + " (" + status +")")
        test.step(function() {
          var source = new EventSource("resources/status-error.py?status=" + status)
          source.onmessage = function() {
            test.step(function() {
              assert_unreached()
            })
            test.done()
          }
          source.onerror = function() {
            test.step(function() {
              assert_equals(this.readyState, this.CLOSED)
            }, this)
            test.done()
          }
        })
      }
      statusTest("204")
      statusTest("205")
      statusTest("210")
      statusTest("299")
      statusTest("404")
      statusTest("410")
      statusTest("503")

