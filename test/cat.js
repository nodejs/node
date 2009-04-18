var filename = ARGV[2];

function cat (file) {
  file.read(100, function (status, data) {
    if (status == 0 && data) {
      stdout.write(data.encodeUtf8());
      cat(file);
    } else {
      file.close();
    }
  });
}

var f = new File;
f.open(filename, "r", function (status) {
  puts("open!");
  if (status == 0)
    cat(f);
  else
    puts("error?");
})
