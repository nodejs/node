var f = new File;

f.open("/tmp/world", "a+", function (status) {
  if (status == 0) {
    node.blocking.print("file opened");
    f.write("hello world\n", function (status, written) {

      node.blocking.print("written. status: " 
                         + status.toString() 
                         + " written: " 
                         + written.toString()
                         );
      f.close();
    });
  } else {
    node.blocking.print("file open failed: " + status.toString());
  }
});
