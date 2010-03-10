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
}
