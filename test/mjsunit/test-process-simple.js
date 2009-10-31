process.mixin(require("./common"));

var cat = process.createChildProcess("cat");

var response = "";
var exit_status = -1;

cat.addListener("output", function (chunk) {
  puts("stdout: " + JSON.stringify(chunk));
  if (chunk) {
    response += chunk;
    if (response === "hello world") {
      puts("closing cat");
      cat.close();
    }
  }
});
cat.addListener("error", function (chunk) {
  puts("stderr: " + JSON.stringify(chunk));
  assertEquals(null, chunk);
});
cat.addListener("exit", function (status) {
  puts("exit event");
  exit_status = status;
});

cat.write("hello");
cat.write(" ");
cat.write("world");

process.addListener("exit", function () {
  assertEquals(0, exit_status);
  assertEquals("hello world", response);
});
