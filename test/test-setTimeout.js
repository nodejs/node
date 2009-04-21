include("mjsunit");

function onLoad () {
  assertInstanceof(setTimeout, Function);
  var starttime = new Date;

  setTimeout(function () {
      var endtime = new Date;
      var diff = endtime - starttime;
      if (diff < 0) diff = -diff;
      assertTrue(900 < diff || diff < 1100);
  }, 1000);

  // this timer shouldn't execute
  var id = setTimeout(function () { assertTrue(false); }, 500);
  clearTimeout(id);
}
