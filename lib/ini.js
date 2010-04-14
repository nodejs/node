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

// ForEaches over an object. The only thing faster is to inline this function.
function objectEach(obj, fn, thisObj) {
  var keys, key, i, length;
  keys = Object.keys(obj);
  length = keys.length;
  for (i = 0; i < length; i++) {
    key = keys[i];
    fn.call(thisObj, obj[key], key, obj);
  }
}

exports.stringify = function (obj) {
  // if the obj has a "-" section, then do that first.
  var ini = [];
  if ("-" in obj) {
    objectEach(obj["-"], function (value, key) {
      ini[ini.length] = safe(key) + " = " + safe(value) + "\n";
    });
  }
  objectEach(obj, function (section, name) {
    if (name === "-") return;
    ini[ini.length] = "[" + safe(name) + "]\n";
    objectEach(section, function (value, key) {
      ini[ini.length] = safe(key) + ((value === true)
        ? "\n"
        : " = "+safe(value)+"\n");
    });
  });
  return ini.join("");
};

exports.encode = exports.stringify;
exports.decode = exports.parse;
