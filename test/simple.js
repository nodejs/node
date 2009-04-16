var f = new File;
f.onError = function (call) {
  node.blocking.print("file error with " + call );
}
f.onOpen = function () {
  node.blocking.print("file opened.");
  f.write("hello world\n", function (status, written) {

    node.blocking.print("written. status: " 
                       + status.toString() 
                       + " written: " 
                       + written.toString()
                       );
    f.close();
  });
};

f.open("/tmp/world", "a+");
