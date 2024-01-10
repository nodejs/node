// META: title=EventSource: cross-origin

      const crossdomain = location.href.replace('://', '://élève.').replace(/\/[^\/]*$/, '/'),
            origin = location.origin.replace('://', '://xn--lve-6lad.');


      function doCORS(url, title) {
        async_test(document.title + " " + title).step(function() {
          var source = new EventSource(url, { withCredentials: true })
          source.onmessage = this.step_func_done(e => {
            assert_equals(e.data, "data");
            assert_equals(e.origin, origin);
            source.close();
          })
        })
      }

      doCORS(crossdomain + "resources/cors.py?run=message",
        "basic use")
      doCORS(crossdomain + "resources/cors.py?run=redirect&location=/eventsource/resources/cors.py?run=message",
        "redirect use")
      doCORS(crossdomain + "resources/cors.py?run=status-reconnect&status=200",
        "redirect use recon")

      function failCORS(url, title) {
        async_test(document.title + " " + title).step(function() {
          var source = new EventSource(url)
          source.onerror = this.step_func(function(e) {
            assert_equals(source.readyState, source.CLOSED, 'readyState')
            assert_false(e.hasOwnProperty('data'))
            source.close()
            this.done()
          })

          /* Shouldn't happen */
          source.onmessage = this.step_func(function(e) {
            assert_unreached("shouldn't fire message event")
          })
          source.onopen = this.step_func(function(e) {
            assert_unreached("shouldn't fire open event")
          })
        })
      }

      failCORS(crossdomain + "resources/cors.py?run=message&origin=http://example.org",
        "allow-origin: http://example.org should fail")
      failCORS(crossdomain + "resources/cors.py?run=message&origin=",
        "allow-origin:'' should fail")
      failCORS(crossdomain + "resources/message.py",
        "No allow-origin should fail")

