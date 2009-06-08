var string = "D";

exports.D = function () {
  return string;
};

function onExit () {
  node.debug("d.js onExit called");
  string = "D done";
}

