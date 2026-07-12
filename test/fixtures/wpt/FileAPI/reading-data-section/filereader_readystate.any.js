// META: title=FileAPI Test: filereader_readystate

    async_test(function() {
      var blob = new Blob(["THIS TEST THE READYSTATE WHEN READ BLOB"]);
      var reader = new FileReader();

      assert_equals(reader.readyState, reader.EMPTY);

      reader.onloadstart = this.step_func(function(evt) {
        assert_equals(reader.readyState, reader.LOADING);
      });

      reader.onloadend = this.step_func(function(evt) {
        assert_equals(reader.readyState, reader.DONE);
        this.done();
      });

      reader.readAsDataURL(blob);
    });
