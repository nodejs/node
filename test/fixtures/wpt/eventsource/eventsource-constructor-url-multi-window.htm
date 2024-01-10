<!DOCTYPE html>
<html>
  <head>
    <title>EventSource: resolving URLs</title>
    <script src="/resources/testharness.js"></script>
    <script src="/resources/testharnessreport.js"></script>
  </head>
  <body>
    <div id="log"></div>
    <script>
      var test = async_test()
      function init() {
        test.step(function() {
          source = new self[0].EventSource("message.py")
          source.onopen = function(e) {
            test.step(function() {
              assert_equals(source.readyState, source.OPEN)
              assert_false(e.hasOwnProperty('data'))
              assert_false(e.bubbles)
              assert_false(e.cancelable)
              this.close()
              test.done()
            }, this)
          }
          source.onerror = function(e) {
            test.step(function() {
              assert_unreached()
              source.close()
              test.done()
            })
          }
        })
      }
    </script>
    <iframe src="resources/init.htm"></iframe>
  </body>
</html>
