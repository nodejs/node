var f = new File;

f.open("/tmp/world", "r+", function (status) {
  if (status == 0) {
    stdout.puts("file open");
    f.write("hello world\n")
    f.write("something else.\n", function () {
      stdout.puts("written. ");
    });
    f.read(100, function (status, buf) {
      stdout.puts("read: >>" + buf.encodeUtf8() + "<<");
    });
  } else {
    stdout.puts("file open failed: " + status.toString());
  }

  f.close(function () { stdout.puts("closed.") });
});
