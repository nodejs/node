File.stat(ARGV[2], function (status, stats) {
    if (status == 0) {
      for (var i in stats) {
        puts(i + " : " + stats[i]);
      }
    } else { 
      puts("error: " + File.strerror(status));
    }
});


File.exists(ARGV[2], function (r) {
  puts("file exists: " + r);
});
