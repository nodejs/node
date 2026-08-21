// Compares a performance entry to a predefined one
// perfEntriesToCheck is an array of performance entries from the user agent
// expectedEntries is an array of performance entries minted by the test
function checkEntries(perfEntriesToCheck, expectedEntries) {
  function findMatch(pe) {
    // we match based on entryType and name
    for (var i = expectedEntries.length - 1; i >= 0; i--) {
      var ex = expectedEntries[i];
      if (ex.entryType === pe.entryType && ex.name === pe.name) {
        return ex;
      }
    }
    return null;
  }

  assert_equals(perfEntriesToCheck.length, expectedEntries.length, "performance entries must match");

  perfEntriesToCheck.forEach(function (pe1) {
    assert_not_equals(findMatch(pe1), null, "Entry matches");
  });
}

// Waits for performance.now to advance. Since precision reduction might
// cause it to return the same value across multiple calls.
function wait() {
  var now = performance.now();
  while (now === performance.now())
    continue;
}

// Ensure the entries list is sorted by startTime.
function checkSorted(entries) {
  assert_not_equals(entries.length, 0, "entries list must not be empty");
  if (!entries.length)
    return;

  var sorted = false;
  var lastStartTime = entries[0].startTime;
  for (var i = 1; i < entries.length; ++i) {
    var currStartTime = entries[i].startTime;
    assert_less_than_equal(lastStartTime, currStartTime, "entry list must be sorted by startTime");
    lastStartTime = currStartTime;
  }
}
