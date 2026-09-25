// META: title=FileAPI Test: filereader_error

    async_test(function() {
      var blob = new Blob(["TEST THE ERROR ATTRIBUTE AND ERROR EVENT"]);
      var reader = new FileReader();
      assert_equals(reader.error, null, "The error is null when no error occurred");

      reader.onload = this.step_func(function(evt) {
        assert_unreached("Should not dispatch the load event");
      });

      reader.onloadend = this.step_func(function(evt) {
        assert_equals(reader.result, null, "The result is null");
        this.done();
      });

      reader.readAsText(blob);
      reader.abort();
    });
