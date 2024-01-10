// META: title=EventSource: credentials
      var crossdomain = location.href
                    .replace('://', '://www2.')
                    .replace(/\/[^\/]*$/, '/')

      function testCookie(desc, success, props, id) {
        var test = async_test(document.title + ': credentials ' + desc)
        test.step(function() {
          var source = new EventSource(crossdomain + "resources/cors-cookie.py?ident=" + id, props)

          source.onmessage = test.step_func(function(e) {
            if(e.data.indexOf("first") == 0) {
              assert_equals(e.data, "first NO_COOKIE", "cookie status")
            }
            else if(e.data.indexOf("second") == 0) {
              if (success)
                assert_equals(e.data, "second COOKIE", "cookie status")
              else
                assert_equals(e.data, "second NO_COOKIE", "cookie status")

              source.close()
              test.done()
            }
            else {
              assert_unreached("unrecognized data returned: " + e.data)
              source.close()
              test.done()
            }
          })
        })
      }

      testCookie('enabled',  true,  { withCredentials: true  }, '1_' + new Date().getTime())
      testCookie('disabled', false, { withCredentials: false }, '2_' + new Date().getTime())
      testCookie('default',  false, { },                        '3_' + new Date().getTime())


