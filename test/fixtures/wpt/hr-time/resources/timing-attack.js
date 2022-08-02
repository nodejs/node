function run_test(isolated) {
  let resolution = 100;
  if (isolated) {
    resolution = 5;
  }
  test(function() {
    function check_resolutions(times, length) {
      const end = length - 2;

      // we compare each value with the following ones
      for (let i = 0; i < end; i++) {
        const h1 = times[i];
        for (let j = i+1; j < end; j++) {
          const h2 = times[j];
          const diff = h2 - h1;
          assert_true((diff === 0) || ((diff * 1000) >= resolution),
            "Differences smaller than ' + resolution + ' microseconds: " + diff);
        }
      }
      return true;
    }

    const times = new Array(10);
    let index = 0;
    let hrt1, hrt2, hrt;
    assert_equals(self.crossOriginIsolated, isolated, "Document cross-origin isolated value matches");

    // rapid firing of performance.now
    hrt1 = performance.now();
    hrt2 = performance.now();
    times[index++] = hrt1;
    times[index++] = hrt2;

    // ensure that we get performance.now() to return a different value
    do {
      hrt = performance.now();
      times[index++] = hrt;
    } while ((hrt - hrt1) === 0);

    assert_true(check_resolutions(times, index), 'Difference should be at least ' + resolution + ' microseconds.');
  }, 'The recommended minimum resolution of the Performance interface has been set to ' + resolution + ' microseconds for cross-origin isolated contexts.');
}
