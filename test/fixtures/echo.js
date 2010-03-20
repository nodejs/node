require("../common");

print("hello world\r\n");

var stdin = process.openStdin();

stdin.addListener("data", function (data) {
  process.stdout.write(data);
});

stdin.addListener("end", function () {
  process.stdout.close();
});
