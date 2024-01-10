// META: title=EventSource: "retry" field (bogus)
      var test = async_test()
      test.step(function() {
        var timeoutms = 3000,
            source = new EventSource("resources/message.py?message=retry%3A3000%0Aretry%3A1000x%0Adata%3Ax"),
            opened = 0
        source.onopen = function() {
          test.step(function() {
            if(opened == 0)
              opened = new Date().getTime()
            else {
              var diff = (new Date().getTime()) - opened
              assert_true(Math.abs(1 - diff / timeoutms) < 0.25) // allow 25% difference
              this.close();
              test.done()
            }
          }, this)
        }
      })
