include("mjsunit");

function on_load () {
  assertInstanceof(setTimeout, Function);
  var starttime = new Date;
  setTimeout(function () {
      var endtime = new Date;
      var diff = endtime - starttime;
      if (diff < 0) diff = -diff;
      assertTrue(900 < diff || diff < 1100);
  }, 1000);
}
