// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


/**
 * This function provides requestAnimationFrame in a cross browser way.
 * http://paulirish.com/2011/requestanimationframe-for-smart-animating/
 */
if ( !window.requestAnimationFrame ) {
  window.requestAnimationFrame = ( function() {
    return window.webkitRequestAnimationFrame ||
        window.mozRequestAnimationFrame ||
        window.oRequestAnimationFrame ||
        window.msRequestAnimationFrame ||
        function(callback, element) {
          window.setTimeout( callback, 1000 / 60 );
        };
  } )();
}

var kNPoints = 8000;
var kNModifications = 20;
var kNVisiblePoints = 200;
var kDecaySpeed = 20;

var kPointRadius = 4;
var kInitialLifeForce = 100;

var livePoints = void 0;
var dyingPoints = void 0;
var scene = void 0;
var renderingStartTime = void 0;
var scene = void 0;
var pausePlot = void 0;
var splayTree = void 0;
var numberOfFrames = 0;
var sumOfSquaredPauses = 0;
var benchmarkStartTime = void 0;
var benchmarkTimeLimit = void 0;
var autoScale = void 0;
var pauseDistribution = [];


function Point(x, y, z, payload) {
  this.x = x;
  this.y = y;
  this.z = z;

  this.next = null;
  this.prev = null;
  this.payload = payload;
  this.lifeForce = kInitialLifeForce;
}


Point.prototype.color = function () {
  return "rgba(0, 0, 0, " + (this.lifeForce / kInitialLifeForce) + ")";
};


Point.prototype.decay = function () {
  this.lifeForce -= kDecaySpeed;
  return this.lifeForce <= 0;
};


function PointsList() {
  this.head = null;
  this.count = 0;
}


PointsList.prototype.add = function (point) {
  if (this.head !== null) this.head.prev = point;
  point.next = this.head;
  this.head = point;
  this.count++;
}


PointsList.prototype.remove = function (point) {
  if (point.next !== null) {
    point.next.prev = point.prev;
  }
  if (point.prev !== null) {
    point.prev.next = point.next;
  } else {
    this.head = point.next;
  }
  this.count--;
}


function GeneratePayloadTree(depth, tag) {
  if (depth == 0) {
    return {
      array  : [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ],
      string : 'String for key ' + tag + ' in leaf node'
    };
  } else {
    return {
      left:  GeneratePayloadTree(depth - 1, tag),
      right: GeneratePayloadTree(depth - 1, tag)
    };
  }
}


// To make the benchmark results predictable, we replace Math.random
// with a 100% deterministic alternative.
Math.random = (function() {
  var seed = 49734321;
  return function() {
    // Robert Jenkins' 32 bit integer hash function.
    seed = seed & 0xffffffff;
    seed = ((seed + 0x7ed55d16) + (seed << 12))  & 0xffffffff;
    seed = ((seed ^ 0xc761c23c) ^ (seed >>> 19)) & 0xffffffff;
    seed = ((seed + 0x165667b1) + (seed << 5))   & 0xffffffff;
    seed = ((seed + 0xd3a2646c) ^ (seed << 9))   & 0xffffffff;
    seed = ((seed + 0xfd7046c5) + (seed << 3))   & 0xffffffff;
    seed = ((seed ^ 0xb55a4f09) ^ (seed >>> 16)) & 0xffffffff;
    return (seed & 0xfffffff) / 0x10000000;
  };
})();


function GenerateKey() {
  // The benchmark framework guarantees that Math.random is
  // deterministic; see base.js.
  return Math.random();
}

function CreateNewPoint() {
  // Insert new node with a unique key.
  var key;
  do { key = GenerateKey(); } while (splayTree.find(key) != null);

  var point = new Point(Math.random() * 40 - 20,
                        Math.random() * 40 - 20,
                        Math.random() * 40 - 20,
                        GeneratePayloadTree(5, "" + key));

  livePoints.add(point);

  splayTree.insert(key, point);
  return key;
}

function ModifyPointsSet() {
  if (livePoints.count < kNPoints) {
    for (var i = 0; i < kNModifications; i++) {
      CreateNewPoint();
    }
  } else if (kNModifications === 20) {
    kNModifications = 80;
    kDecay = 30;
  }

  for (var i = 0; i < kNModifications; i++) {
    var key = CreateNewPoint();
    var greatest = splayTree.findGreatestLessThan(key);
    if (greatest == null) {
      var point = splayTree.remove(key).value;
    } else {
      var point = splayTree.remove(greatest.key).value;
    }
    livePoints.remove(point);
    point.payload = null;
    dyingPoints.add(point);
  }
}


function PausePlot(width, height, size, scale) {
  var canvas = document.createElement("canvas");
  canvas.width = this.width = width;
  canvas.height = this.height = height;
  document.body.appendChild(canvas);

  this.ctx = canvas.getContext('2d');

  if (typeof scale !== "number") {
    this.autoScale = true;
    this.maxPause = 0;
  } else {
    this.autoScale = false;
    this.maxPause = scale;
  }

  this.size = size;

  // Initialize cyclic buffer for pauses.
  this.pauses = new Array(this.size);
  this.start = this.size;
  this.idx = 0;
}


PausePlot.prototype.addPause = function (p) {
  if (this.idx === this.size) {
    this.idx = 0;
  }

  if (this.idx === this.start) {
    this.start++;
  }

  if (this.start === this.size) {
    this.start = 0;
  }

  this.pauses[this.idx++] = p;
};


PausePlot.prototype.iteratePauses = function (f) {
  if (this.start < this.idx) {
    for (var i = this.start; i < this.idx; i++) {
      f.call(this, i - this.start, this.pauses[i]);
    }
  } else {
    for (var i = this.start; i < this.size; i++) {
      f.call(this, i - this.start, this.pauses[i]);
    }

    var offs = this.size - this.start;
    for (var i = 0; i < this.idx; i++) {
      f.call(this, i + offs, this.pauses[i]);
    }
  }
};


PausePlot.prototype.draw = function () {
  var first = null;

  if (this.autoScale) {
    this.iteratePauses(function (i, v) {
      if (first === null) {
        first = v;
      }
      this.maxPause = Math.max(v, this.maxPause);
    });
  }

  var dx = this.width / this.size;
  var dy = this.height / this.maxPause;

  this.ctx.save();
  this.ctx.clearRect(0, 0, this.width, this.height);
  this.ctx.beginPath();
  this.ctx.moveTo(1, dy * this.pauses[this.start]);
  var p = first;
  this.iteratePauses(function (i, v) {
    var delta = v - p;
    var x = 1 + dx * i;
    var y = dy * v;
    this.ctx.lineTo(x, y);
    if (delta > 2 * (p / 3)) {
      this.ctx.font = "bold 12px sans-serif";
      this.ctx.textBaseline = "bottom";
      this.ctx.fillText(v + "ms", x + 2, y);
    }
    p = v;
  });
  this.ctx.strokeStyle = "black";
  this.ctx.stroke();
  this.ctx.restore();
}


function Scene(width, height) {
  var canvas = document.createElement("canvas");
  canvas.width = width;
  canvas.height = height;
  document.body.appendChild(canvas);

  this.ctx = canvas.getContext('2d');
  this.width = canvas.width;
  this.height = canvas.height;

  // Projection configuration.
  this.x0 = canvas.width / 2;
  this.y0 = canvas.height / 2;
  this.z0 = 100;
  this.f  = 1000;  // Focal length.

  // Camera is rotating around y-axis.
  this.angle = 0;
}


Scene.prototype.drawPoint = function (x, y, z, color) {
  // Rotate the camera around y-axis.
  var rx = x * Math.cos(this.angle) - z * Math.sin(this.angle);
  var ry = y;
  var rz = x * Math.sin(this.angle) + z * Math.cos(this.angle);

  // Perform perspective projection.
  var px = (this.f * rx) / (rz - this.z0) + this.x0;
  var py = (this.f * ry) / (rz - this.z0) + this.y0;

  this.ctx.save();
  this.ctx.fillStyle = color
  this.ctx.beginPath();
  this.ctx.arc(px, py, kPointRadius, 0, 2 * Math.PI, true);
  this.ctx.fill();
  this.ctx.restore();
};


Scene.prototype.drawDyingPoints = function () {
  var point_next = null;
  for (var point = dyingPoints.head; point !== null; point = point_next) {
    // Rotate the scene around y-axis.
    scene.drawPoint(point.x, point.y, point.z, point.color());

    point_next = point.next;

    // Decay the current point and remove it from the list
    // if it's life-force ran out.
    if (point.decay()) {
      dyingPoints.remove(point);
    }
  }
};


Scene.prototype.draw = function () {
  this.ctx.save();
  this.ctx.clearRect(0, 0, this.width, this.height);
  this.drawDyingPoints();
  this.ctx.restore();

  this.angle += Math.PI / 90.0;
};


function updateStats(pause) {
  numberOfFrames++;
  if (pause > 20) {
    sumOfSquaredPauses += (pause - 20) * (pause - 20);
  }
  pauseDistribution[Math.floor(pause / 10)] |= 0;
  pauseDistribution[Math.floor(pause / 10)]++;
}


function renderStats() {
  var msg = document.createElement("p");
  msg.innerHTML = "Score " +
    Math.round(numberOfFrames * 1000 / sumOfSquaredPauses);
  var table = document.createElement("table");
  table.align = "center";
  for (var i = 0; i < pauseDistribution.length; i++) {
    if (pauseDistribution[i] > 0) {
      var row = document.createElement("tr");
      var time = document.createElement("td");
      var count = document.createElement("td");
      time.innerHTML = i*10 + "-" + (i+1)*10 + "ms";
      count.innerHTML = " => " + pauseDistribution[i];
      row.appendChild(time);
      row.appendChild(count);
      table.appendChild(row);
    }
  }
  div.appendChild(msg);
  div.appendChild(table);
}


function render() {
  if (typeof renderingStartTime === 'undefined') {
    renderingStartTime = Date.now();
    benchmarkStartTime = renderingStartTime;
  }

  ModifyPointsSet();

  scene.draw();

  var renderingEndTime = Date.now();
  var pause = renderingEndTime - renderingStartTime;
  pausePlot.addPause(pause);
  renderingStartTime = renderingEndTime;

  pausePlot.draw();

  updateStats(pause);

  div.innerHTML =
      livePoints.count + "/" + dyingPoints.count + " " +
      pause + "(max = " + pausePlot.maxPause + ") ms " +
      numberOfFrames + " frames";

  if (renderingEndTime < benchmarkStartTime + benchmarkTimeLimit) {
    // Schedule next frame.
    requestAnimationFrame(render);
  } else {
    renderStats();
  }
}


function Form() {
  function create(tag) { return document.createElement(tag); }
  function text(value) { return document.createTextNode(value); }

  this.form = create("form");
  this.form.setAttribute("action", "javascript:start()");

  var table = create("table");
  table.setAttribute("style", "margin-left: auto; margin-right: auto;");

  function col(a) {
    var td = create("td");
    td.appendChild(a);
    return td;
  }

  function row(a, b) {
    var tr = create("tr");
    tr.appendChild(col(a));
    tr.appendChild(col(b));
    return tr;
  }

  this.timelimit = create("input");
  this.timelimit.setAttribute("value", "60");

  table.appendChild(row(text("Time limit in seconds"), this.timelimit));

  this.autoscale = create("input");
  this.autoscale.setAttribute("type", "checkbox");
  this.autoscale.setAttribute("checked", "true");
  table.appendChild(row(text("Autoscale pauses plot"), this.autoscale));

  var button = create("input");
  button.setAttribute("type", "submit");
  button.setAttribute("value", "Start");
  this.form.appendChild(table);
  this.form.appendChild(button);

  document.body.appendChild(this.form);
}


Form.prototype.remove = function () {
  document.body.removeChild(this.form);
};


function init() {
  livePoints = new PointsList;
  dyingPoints = new PointsList;

  splayTree = new SplayTree();

  scene = new Scene(640, 480);

  div = document.createElement("div");
  document.body.appendChild(div);

  pausePlot = new PausePlot(480, autoScale ? 240 : 500, 160, autoScale ? void 0 : 500);
}

function start() {
  benchmarkTimeLimit = form.timelimit.value * 1000;
  autoScale = form.autoscale.checked;
  form.remove();
  init();
  render();
}

var form = new Form();
