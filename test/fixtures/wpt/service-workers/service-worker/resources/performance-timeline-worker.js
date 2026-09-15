importScripts('/resources/testharness.js');

promise_test(function(test) {
    var durationMsec = 100;
    // There are limits to our accuracy here.  Timers may fire up to a
    // millisecond early due to platform-dependent rounding.  In addition
    // the performance API introduces some rounding as well to prevent
    // timing attacks.
    var accuracy = 1.5;
    return new Promise(function(resolve) {
        performance.mark('startMark');
        setTimeout(resolve, durationMsec);
      }).then(function() {
          performance.mark('endMark');
          performance.measure('measure', 'startMark', 'endMark');
          var startMark = performance.getEntriesByName('startMark')[0];
          var endMark = performance.getEntriesByName('endMark')[0];
          var measure = performance.getEntriesByType('measure')[0];
          assert_equals(measure.startTime, startMark.startTime);
          assert_approx_equals(endMark.startTime - startMark.startTime,
                               measure.duration, 0.001);
          assert_greater_than(measure.duration, durationMsec - accuracy);
          assert_equals(performance.getEntriesByType('mark').length, 2);
          assert_equals(performance.getEntriesByType('measure').length, 1);
          performance.clearMarks('startMark');
          performance.clearMeasures('measure');
          assert_equals(performance.getEntriesByType('mark').length, 1);
          assert_equals(performance.getEntriesByType('measure').length, 0);
      });
  }, 'User Timing');

promise_test(function(test) {
    return fetch('sample.txt')
      .then(function(resp) {
          return resp.text();
        })
      .then(function(text) {
          var expectedResources = ['testharness.js', 'sample.txt'];
          assert_equals(performance.getEntriesByType('resource').length, expectedResources.length);
          for (var i = 0; i < expectedResources.length; i++) {
              var entry = performance.getEntriesByType('resource')[i];
              assert_true(entry.name.endsWith(expectedResources[i]));
              assert_equals(entry.workerStart, 0);
              assert_greater_than(entry.startTime, 0);
              assert_greater_than(entry.responseEnd, entry.startTime);
          }
          return new Promise(function(resolve) {
              performance.onresourcetimingbufferfull = _ => {
                resolve('bufferfull');
              }
              performance.setResourceTimingBufferSize(expectedResources.length);
              fetch('sample.txt');
          });
        })
      .then(function(result) {
          assert_equals(result, 'bufferfull');
          performance.clearResourceTimings();
          assert_equals(performance.getEntriesByType('resource').length, 0);
        })
  }, 'Resource Timing');

done();
