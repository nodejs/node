
var fs = require("fs"),
  sys = require("sys"),
  path = require("path"),
  xml = fs.cat(path.join(__dirname, "test.xml")),
  sax = require("../lib/sax"),
  strict = sax.parser(true),
  loose = sax.parser(false, {trim:true}),
  inspector = function (ev) { return function (data) {
    // sys.error("");
    // sys.error(ev+": "+sys.inspect(data));
    // for (var i in data) sys.error(i+ " "+sys.inspect(data[i]));
    // sys.error(this.line+":"+this.column);
  }};

xml.addCallback(function (xml) {
  // strict.write(xml);
  
  sax.EVENTS.forEach(function (ev) {
    loose["on"+ev] = inspector(ev);
  });
  loose.onend = function () {
    // sys.error("end");
    // sys.error(sys.inspect(loose));
  };
  
  // do this one char at a time to verify that it works.
  // (function () {
  //   if (xml) {
  //     loose.write(xml.substr(0,1000));
  //     xml = xml.substr(1000);
  //     process.nextTick(arguments.callee);
  //   } else loose.close();
  // })();
  
  for (var i = 0; i < 1000; i ++) {
    loose.write(xml);
    loose.close();
  }

});
