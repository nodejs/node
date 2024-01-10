// META: title=EventSource: stringify argument

      async_test(function (test) {
        test.step(function() {
          var source = new EventSource({toString:function(){return "resources/message.py";}})
          source.onopen = function(e) {
            test.step(function() {
              assert_false(e.hasOwnProperty('data'))
              source.close()
              test.done()
            })
          }
        });
      }, document.title + ', object');

      test(function(){
        var source = new EventSource(1);
        assert_regexp_match(source.url, /\/1$/);
      }, document.title + ', 1');
      test(function(){
        var source = new EventSource(null);
        assert_regexp_match(source.url, /\/null$/);
      }, document.title + ', null');
      test(function(){
        var source = new EventSource(undefined);
        assert_regexp_match(source.url, /\/undefined$/);
      }, document.title + ', undefined');

