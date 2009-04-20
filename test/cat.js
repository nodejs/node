var filename = ARGV[2];
File.cat(filename, function (status, content) {
  if (status == 0) 
    puts(content);
  else
    puts("error");
});
puts("hello world");
