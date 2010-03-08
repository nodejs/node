require("../common");
process.stdio.open();

print("hello world\r\n");

process.stdio.addListener("data", function (data) {
  print(data);
});

process.stdio.addListener("close", function () {
  process.stdio.close();
});
