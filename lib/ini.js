exports.parse = function(d) {
  var trim = function(str) { return str.replace(/^\s\s*/, '').replace(/\s\s*$/, ''); }
  var ini = {'-':{}};

  var section = '-';

  var lines = d.split('\n');
  for (var i=0; i<lines.length; i++) {

    var re = /(.*)=(.*)|\[([a-z:\.0-9_\s]+)\]/i;

    var match = lines[i].match(re);
    if (match != null) {
      if (match[3] != undefined) {
        section = match[3];
        ini[section] = {};
      } else {
        var key = trim(match[1]);
        var value = trim(match[2]);
        ini[section][key] = value;
      }
    }
  }

  return ini;
}
