// META: title=FileAPI Test: filereader_readAsText

    async_test(function() {
      var blob = new Blob(["TEST"]);
      var reader = new FileReader();

      reader.onload = this.step_func(function(evt) {
        assert_equals(typeof reader.result, "string", "The result is typeof string");
        assert_equals(reader.result, "TEST", "The result is TEST");
        this.done();
      });

      reader.onloadstart = this.step_func(function(evt) {
        assert_equals(reader.readyState, reader.LOADING, "The readyState");
      });

      reader.onprogress = this.step_func(function(evt) {
        assert_equals(reader.readyState, reader.LOADING);
      });

      reader.readAsText(blob);
    }, "readAsText should correctly read UTF-8.");

    async_test(function() {
      var blob = new Blob(["TEST"]);
      var reader = new FileReader();
      var reader_UTF16 = new FileReader();
      reader_UTF16.onload = this.step_func(function(evt) {
        // "TEST" in UTF-8 is 0x54 0x45 0x53 0x54.
        // Decoded as utf-16 (little-endian), we get 0x4554 0x5453.
        assert_equals(reader_UTF16.readyState, reader.DONE, "The readyState");
        assert_equals(reader_UTF16.result, "\u4554\u5453", "The result is not TEST");
        this.done();
      });
      reader_UTF16.readAsText(blob, "UTF-16");
    }, "readAsText should correctly read UTF-16.");
