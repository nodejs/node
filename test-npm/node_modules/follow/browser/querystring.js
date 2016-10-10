define([], function() {
  var exports = {};

  exports.parse = function(str) {
    var result = {};

    str = str || "";
    str = str.replace(/^\?/, "");
    if(!str) return result;

    var kvs = str.split('&');
    kvs.forEach(function(pair) {
      var both = pair.split('=');
      result[both[0]] = both[1];
    })

    return result;
  }

  exports.stringify = function(query) {
    var result = [];
    for (var k in query)
      result.push(k + '=' + encodeURIComponent(query[k]));
    return result.join('&');
  }

  return exports;
})
