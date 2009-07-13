function next (i) {
  if (i <= 0) return;

  var process = node.createProcess("echo hello");

  process.addListener("output", function (chunk) {
    if (chunk) print(chunk);
  });

  process.addListener("exit", function (code) {
    if (code != 0) node.exit(-1);
    next(i - 1);
  });
}

next(500);
