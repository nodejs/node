// META: title=FileAPI Test: filereader_result

    var blob, blob2;
    setup(function() {
      blob = new Blob(["This test the result attribute"]);
      blob2 = new Blob(["This is a second blob"]);
    });

    async_test(function() {
      var readText = new FileReader();
      assert_equals(readText.result, null);

      readText.onloadend = this.step_func(function(evt) {
        assert_equals(typeof readText.result, "string", "The result type is string");
        assert_equals(readText.result, "This test the result attribute", "The result is correct");
        this.done();
      });

      readText.readAsText(blob);
    }, "readAsText");

    async_test(function() {
      var readDataURL = new FileReader();
      assert_equals(readDataURL.result, null);

      readDataURL.onloadend = this.step_func(function(evt) {
        assert_equals(typeof readDataURL.result, "string", "The result type is string");
        assert_true(readDataURL.result.indexOf("VGhpcyB0ZXN0IHRoZSByZXN1bHQgYXR0cmlidXRl") != -1, "return the right base64 string");
        this.done();
      });

      readDataURL.readAsDataURL(blob);
    }, "readAsDataURL");

    async_test(function() {
      var readArrayBuffer = new FileReader();
      assert_equals(readArrayBuffer.result, null);

      readArrayBuffer.onloadend = this.step_func(function(evt) {
        assert_true(readArrayBuffer.result instanceof ArrayBuffer, "The result is instanceof ArrayBuffer");
        this.done();
      });

      readArrayBuffer.readAsArrayBuffer(blob);
    }, "readAsArrayBuffer");

    async_test(function() {
      var readBinaryString = new FileReader();
      assert_equals(readBinaryString.result, null);

      readBinaryString.onloadend = this.step_func(function(evt) {
        assert_equals(typeof readBinaryString.result, "string", "The result type is string");
        assert_equals(readBinaryString.result, "This test the result attribute", "The result is correct");
        this.done();
      });

      readBinaryString.readAsBinaryString(blob);
    }, "readAsBinaryString");


    for (let event of ['loadstart', 'progress']) {
      for (let method of ['readAsText', 'readAsDataURL', 'readAsArrayBuffer', 'readAsBinaryString']) {
        promise_test(async function(t) {
          var reader = new FileReader();
          assert_equals(reader.result, null, 'result is null before read');

          var eventWatcher = new EventWatcher(t, reader,
              [event, 'loadend']);

          reader[method](blob);
          assert_equals(reader.result, null, 'result is null after first read call');
          await eventWatcher.wait_for(event);
          assert_equals(reader.result, null, 'result is null during event');
          await eventWatcher.wait_for('loadend');
          assert_not_equals(reader.result, null);
          reader[method](blob);
          assert_equals(reader.result, null, 'result is null after second read call');
          await eventWatcher.wait_for(event);
          assert_equals(reader.result, null, 'result is null during second read event');
        }, 'result is null during "' + event + '" event for ' + method);
      }
    }
