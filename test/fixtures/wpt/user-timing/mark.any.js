// test data
var testThreshold = 20;

var expectedTimes = new Array();

function match_entries(entries, index)
{
    var entry = entries[index];
    var match = self.performance.getEntriesByName("mark")[index];
    assert_equals(entry.name, match.name, "entry.name");
    assert_equals(entry.startTime, match.startTime, "entry.startTime");
    assert_equals(entry.entryType, match.entryType, "entry.entryType");
    assert_equals(entry.duration, match.duration, "entry.duration");
}

function filter_entries_by_type(entryList, entryType)
{
    var testEntries = new Array();

    // filter entryList
    for (var i in entryList)
    {
        if (entryList[i].entryType == entryType)
        {
            testEntries.push(entryList[i]);
        }
    }

    return testEntries;
}

test(function () {
  // create first mark
  self.performance.mark("mark");

  expectedTimes[0] = self.performance.now();

  const entries = self.performance.getEntriesByName("mark");
  assert_equals(entries.length, 1);
}, "Entry 0 is properly created");

test(function () {
    // create second, duplicate mark
    self.performance.mark("mark");

    expectedTimes[1] = self.performance.now();

    const entries = self.performance.getEntriesByName("mark");
    assert_equals(entries.length, 2);

}, "Entry 1 is properly created");

function test_mark(index) {
   test(function () {
       const entries = self.performance.getEntriesByName("mark");
       assert_equals(entries[index].name, "mark", "Entry has the proper name");
   }, "Entry " + index + " has the proper name");

   test(function () {
       const entries = self.performance.getEntriesByName("mark");
       assert_approx_equals(entries[index].startTime, expectedTimes[index], testThreshold);
   }, "Entry " + index + " startTime is approximately correct (up to " + testThreshold +
              "ms difference allowed)");

   test(function () {
       const entries = self.performance.getEntriesByName("mark");
       assert_equals(entries[index].entryType, "mark");
   }, "Entry " + index + " has the proper entryType");

   test(function () {
       const entries = self.performance.getEntriesByName("mark");
       assert_equals(entries[index].duration, 0);
   }, "Entry " + index +  " duration == 0");

   test(function () {
    const entries = self.performance.getEntriesByName("mark", "mark");
    assert_equals(entries[index].name, "mark");
   }, "getEntriesByName(\"mark\", \"mark\")[" + index + "] returns an " +
                "object containing a \"mark\" mark");

   test(function () {
    const entries = self.performance.getEntriesByName("mark", "mark");
    match_entries(entries, index);
   }, "The mark returned by getEntriesByName(\"mark\", \"mark\")[" + index
      + "] matches the mark returned by " +
              "getEntriesByName(\"mark\")[" + index + "]");

   test(function () {
    const entries = filter_entries_by_type(self.performance.getEntries(), "mark");
    assert_equals(entries[index].name, "mark");
   }, "getEntries()[" + index + "] returns an " +
                "object containing a \"mark\" mark");

   test(function () {
    const entries = filter_entries_by_type(self.performance.getEntries(), "mark");
    match_entries(entries, index);
   }, "The mark returned by getEntries()[" + index
      + "] matches the mark returned by " +
              "getEntriesByName(\"mark\")[" + index + "]");

   test(function () {
    const entries = self.performance.getEntriesByType("mark");
    assert_equals(entries[index].name, "mark");
   }, "getEntriesByType(\"mark\")[" + index + "] returns an " +
                "object containing a \"mark\" mark");

   test(function () {
    const entries = self.performance.getEntriesByType("mark");
    match_entries(entries, index);
   }, "The mark returned by getEntriesByType(\"mark\")[" + index
      + "] matches the mark returned by " +
              "getEntriesByName(\"mark\")[" + index + "]");

}

for (var i = 0; i < expectedTimes.length; i++) {
  test_mark(i);
}
