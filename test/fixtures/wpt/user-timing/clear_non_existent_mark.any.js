test(function() {
    self.performance.mark("mark1");
    self.performance.mark("mark2");

    // test that two marks have been created
    var entries = self.performance.getEntriesByType("mark");
    assert_equals(entries.length, 2, "Two marks have been created for this test.");

    // clear non-existent mark
    self.performance.clearMarks("mark3");

    // test that "mark1" still exists
    entries = self.performance.getEntriesByName("mark1");
    assert_equals(entries[0].name, "mark1",
              "After a call to self.performance.clearMarks(\"mark3\"), where \"mark3" +
              "\" is a non-existent mark, self.performance.getEntriesByName(\"mark1\") " +
              "returns an object containing the \"mark1\" mark.");

    // test that "mark2" still exists
    entries = self.performance.getEntriesByName("mark2");
    assert_equals(entries[0].name, "mark2",
              "After a call to self.performance.clearMarks(\"mark3\"), where \"mark3" +
              "\" is a non-existent mark, self.performance.getEntriesByName(\"mark2\") " +
              "returns an object containing the \"mark2\" mark.");

}, "Clearing a non-existent mark doesn't affect existing marks");
