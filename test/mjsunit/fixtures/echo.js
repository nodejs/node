process.mixin(require("../common"));
process.stdio.open();
process.stdio.addListener("data", function (data) {
  puts(data);
});