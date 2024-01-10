// META: title=EventSource: url

      test(function() {
        var url = "resources/message.py",
            source = new EventSource(url)
        assert_equals(source.url.substr(-(url.length)), url)
        source.close()
      })
