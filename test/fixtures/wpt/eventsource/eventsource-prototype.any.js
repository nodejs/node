// META: title=EventSource: prototype et al

      test(function() {
        EventSource.prototype.ReturnTrue = function() { return true }
        var source = new EventSource("resources/message.py")
        assert_true(source.ReturnTrue())
        assert_own_property(self, "EventSource")
        source.close()
      })

