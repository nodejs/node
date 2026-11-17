// META: title=FileAPI Test: filereader_abort

    test(function() {
      var readerNoRead = new FileReader();
      readerNoRead.abort();
      assert_equals(readerNoRead.readyState, readerNoRead.EMPTY);
      assert_equals(readerNoRead.result, null);
    }, "Aborting before read");

    promise_test(t => {
        var blob = new Blob(["TEST THE ABORT METHOD"]);
        var readerAbort = new FileReader();

        var eventWatcher = new EventWatcher(t, readerAbort,
            ['abort', 'loadstart', 'loadend', 'error', 'load']);

        // EventWatcher doesn't let us inspect the state after the abort event,
        // so add an extra event handler for that.
        readerAbort.addEventListener('abort', t.step_func(e => {
              assert_equals(readerAbort.readyState, readerAbort.DONE);
          }));

        readerAbort.readAsText(blob);
        return eventWatcher.wait_for('loadstart')
          .then(() => {
              assert_equals(readerAbort.readyState, readerAbort.LOADING);
              // 'abort' and 'loadend' events are dispatched synchronously, so
              // call wait_for before calling abort.
              var nextEvent = eventWatcher.wait_for(['abort', 'loadend']);
              readerAbort.abort();
              return nextEvent;
            })
          .then(() => {
              // https://www.w3.org/Bugs/Public/show_bug.cgi?id=24401
              assert_equals(readerAbort.result, null);
              assert_equals(readerAbort.readyState, readerAbort.DONE);
            });
      }, "Aborting after read");

    promise_test(t => {
        var reader = new FileReader();

        var eventWatcher = new EventWatcher(t, reader,
            ['abort', 'loadstart', 'loadend', 'error', 'load']);

        // First read: run to completion so the reader reaches the DONE state.
        reader.readAsText(new Blob(["first read"]));
        return eventWatcher.wait_for('loadstart')
          .then(() => eventWatcher.wait_for('load'))
          .then(() => eventWatcher.wait_for('loadend'))
          .then(() => {
              assert_equals(reader.readyState, reader.DONE);

              // Reuse the same reader for a second read and abort it while it is
              // still LOADING. 'abort' and 'loadend' are dispatched synchronously,
              // so call wait_for before calling abort.
              reader.readAsText(new Blob(["second read"]));
              assert_equals(reader.readyState, reader.LOADING);
              var nextEvent = eventWatcher.wait_for(['abort', 'loadend']);
              reader.abort();
              return nextEvent;
            })
          .then(() => {
              assert_equals(reader.result, null);
              assert_equals(reader.readyState, reader.DONE);
            });
      }, "Aborting a reused reader after a previous read completed");
