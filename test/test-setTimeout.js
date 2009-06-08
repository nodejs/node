include("mjsunit.js");

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

  var count = 0;
  setInterval(function () {
      count += 1;
      var endtime = new Date;
      var diff = endtime - starttime;
      if (diff < 0) diff = -diff;
      puts(diff);
      var t = count * 1000;
      assertTrue(t - 100 < diff || diff < t + 100);
      assertTrue(count <= 3);
      if (count == 3)
        clearInterval(this);
  }, 1000);
}
