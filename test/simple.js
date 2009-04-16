var f = new File;

f.open("/tmp/world", "a+", function (status) {
  if (status == 0) {
    stdout.puts("file open");
    f.write("hello world\n", function (status, written) {

      stdout.puts("written. status: " 
                 + status.toString() 
                 + " written: " 
                 + written.toString()
                 );
      f.close();
    });
  } else {
    stdout.puts("file open failed: " + status.toString());
  }
});
