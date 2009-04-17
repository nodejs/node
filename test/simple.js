var f = new File;

f.open("/tmp/world", "w+", function (status) {
  if (status == 0) {
    stdout.puts("file open");
    f.write("hello world\n");
    f.write("something else.\n", function () {
      stdout.puts("written. ");
    });
    f.read(100, function (status, buf) {
      if (buf)
        stdout.puts("read: >>" + buf.encodeUtf8() + "<<");
    });
  } else {
    stdout.puts("file open failed: " + status.toString());
  }

  f.close(function (status) { 
    stdout.puts("closed: " + status.toString());
    File.rename("/tmp/world", "/tmp/hello", function (status) {
      stdout.puts("rename: " + status.toString());
    });
  });
});
