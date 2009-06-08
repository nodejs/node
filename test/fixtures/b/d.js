var string = "D";

exports.D = function () {
  return string;
};

function onExit () {
  string = "D done";
}

