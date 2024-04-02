test(function () {
   self.performance.mark("existing_mark");
   var entries = self.performance.getEntriesByName("existing_mark");
   assert_equals(entries.length, 1);
   self.performance.measure("measure", "existing_mark");
}, "Create a mark \"existing_mark\"");
test(function () {
   assert_throws_dom("SyntaxError", function () {
       self.performance.measure("measure", "mark");
   });
}, "self.performance.measure(\"measure\", \"mark\"), where \"mark\" is a non-existent mark, " +
                          "throws a SyntaxError exception.");

test(function () {
   assert_throws_dom("SyntaxError", function () {
       self.performance.measure("measure", "mark", "existing_mark");
   });
}, "self.performance.measure(\"measure\", \"mark\", \"existing_mark\"), where \"mark\" is a " +
                          "non-existent mark, throws a SyntaxError exception.");

test(function () {
   assert_throws_dom("SyntaxError", function () {
       self.performance.measure("measure", "existing_mark", "mark");
   });
}, "self.performance.measure(\"measure\", \"existing_mark\", \"mark\"), where \"mark\" " +
                            "is a non-existent mark, throws a SyntaxError exception.");

test(function () {
   assert_throws_dom("SyntaxError", function () {
       self.performance.measure("measure", "mark", "mark");
   });
}, "self.performance.measure(\"measure\", \"mark\", \"mark\"), where \"mark\" is a " +
                          "non-existent mark, throws a SyntaxError exception.");
