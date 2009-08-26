var string = "D";

exports.D = function () {
  return string;
};

process.addListener("exit", function () {
  string = "D done";
});

