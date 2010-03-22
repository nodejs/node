// TODO:
// 1. Handle quoted strings, including line breaks, so that this:
//    foo = "bar
//      baz"
//    parses to {foo:"bar\n  baz"}
// 2. Escape with \, so that this:
//    foo = bar\
//      \"baz
//    parses to {foo:"bar\n  \"baz"}

exports.parse = function(d) {
  var ini = {'-':{}};

  var section = '-';

  var lines = d.split('\n');
  for (var i=0; i<lines.length; i++) {
    var line = lines[i].trim(),
      rem = line.indexOf(";");

    if (rem !== -1) line = line.substr(0, rem);

    var re = /^\[([^\]]*)\]$|^([^=]+)(=(.*))?$/i;

    var match = line.match(re);
    if (match != null) {
      if (match[1] != undefined) {
        section = match[1].trim();
        ini[section] = {};
      } else {
        var key = match[2].trim(),
          value = (match[3]) ? (match[4] || "").trim() : true;
        ini[section][key] = value;
      }
    }
  }

  return ini;
};

function safe (val) {
  return (val+"").replace(/[\n\r]+/g, " ");
}

exports.stringify = function (obj) {
  // if the obj has a "-" section, then do that first.
  var ini = "";
  if ("-" in obj) {
    for (var key in obj["-"]) {
      ini += safe(key)+" = "+safe(obj["-"][key])+"\n";
    }
  }
  for (var section in obj) if (section !== "-") {
    ini += "[" + safe(section) + "]\n";
    for (var key in obj[section]) {

      ini += safe(key) + ((obj[section][key] === true)
        ? "\n"
        : " = "+safe(obj[section][key])+"\n");
    }
  }
  return ini;
};

exports.encode = exports.stringify;
exports.decode = exports.parse;
