onerror = function(message, location, line, col, error) {
  postMessage({ source: "onerror", value: error });
}

addEventListener("error", function(e) {
  postMessage({ source: "event listener", value: e.error });
});

throw "hello";
