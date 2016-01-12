/*
 Copyright (C) 2007 Apple Inc.  All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:
 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
*/

function record(time) {
    document.getElementById("console").innerHTML = time + "ms";
    if (window.parent) {
        parent.recordResult(time);
    }
}

var _sunSpiderStartDate = new Date();

/* The Great Computer Language Shootout
   http://shootout.alioth.debian.org/
   contributed by Isaac Gouy */

var PI = 3.141592653589793;
var SOLAR_MASS = 4 * PI * PI;
var DAYS_PER_YEAR = 365.24;

function Body(x,y,z,vx,vy,vz,mass){
   this.x = x;
   this.y = y;
   this.z = z;
   this.vx = vx;
   this.vy = vy;
   this.vz = vz;
   this.mass = mass;
}

Body.prototype.offsetMomentum = function(px,py,pz) {
   this.vx = -px / SOLAR_MASS;
   this.vy = -py / SOLAR_MASS;
   this.vz = -pz / SOLAR_MASS;
   return this;
}

function Jupiter(){
   return new Body(
      4.84143144246472090e+00,
      -1.16032004402742839e+00,
      -1.03622044471123109e-01,
      1.66007664274403694e-03 * DAYS_PER_YEAR,
      7.69901118419740425e-03 * DAYS_PER_YEAR,
      -6.90460016972063023e-05 * DAYS_PER_YEAR,
      9.54791938424326609e-04 * SOLAR_MASS
   );
}

function Saturn(){
   return new Body(
      8.34336671824457987e+00,
      4.12479856412430479e+00,
      -4.03523417114321381e-01,
      -2.76742510726862411e-03 * DAYS_PER_YEAR,
      4.99852801234917238e-03 * DAYS_PER_YEAR,
      2.30417297573763929e-05 * DAYS_PER_YEAR,
      2.85885980666130812e-04 * SOLAR_MASS
   );
}

function Uranus(){
   return new Body(
      1.28943695621391310e+01,
      -1.51111514016986312e+01,
      -2.23307578892655734e-01,
      2.96460137564761618e-03 * DAYS_PER_YEAR,
      2.37847173959480950e-03 * DAYS_PER_YEAR,
      -2.96589568540237556e-05 * DAYS_PER_YEAR,
      4.36624404335156298e-05 * SOLAR_MASS
   );
}

function Neptune(){
   return new Body(
      1.53796971148509165e+01,
      -2.59193146099879641e+01,
      1.79258772950371181e-01,
      2.68067772490389322e-03 * DAYS_PER_YEAR,
      1.62824170038242295e-03 * DAYS_PER_YEAR,
      -9.51592254519715870e-05 * DAYS_PER_YEAR,
      5.15138902046611451e-05 * SOLAR_MASS
   );
}

function Sun(){
   return new Body(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, SOLAR_MASS);
}


function NBodySystem(bodies){
   this.bodies = bodies;
   var px = 0.0;
   var py = 0.0;
   var pz = 0.0;
   var size = this.bodies.length;
   for (var i=0; i<size; i++){
      var b = this.bodies[i];
      var m = b.mass;
      px += b.vx * m;
      py += b.vy * m;
      pz += b.vz * m;
   }
   this.bodies[0].offsetMomentum(px,py,pz);
}

NBodySystem.prototype.advance = function(dt){
   var dx, dy, dz, distance, mag;
   var size = this.bodies.length;

   for (var i=0; i<size; i++) {
      var bodyi = this.bodies[i];
      for (var j=i+1; j<size; j++) {
         var bodyj = this.bodies[j];
         dx = bodyi.x - bodyj.x;
         dy = bodyi.y - bodyj.y;
         dz = bodyi.z - bodyj.z;

         distance = Math.sqrt(dx*dx + dy*dy + dz*dz);
         mag = dt / (distance * distance * distance);

         bodyi.vx -= dx * bodyj.mass * mag;
         bodyi.vy -= dy * bodyj.mass * mag;
         bodyi.vz -= dz * bodyj.mass * mag;

         bodyj.vx += dx * bodyi.mass * mag;
         bodyj.vy += dy * bodyi.mass * mag;
         bodyj.vz += dz * bodyi.mass * mag;
      }
   }

   for (var i=0; i<size; i++) {
      var body = this.bodies[i];
      body.x += dt * body.vx;
      body.y += dt * body.vy;
      body.z += dt * body.vz;
   }
}

NBodySystem.prototype.energy = function(){
   var dx, dy, dz, distance;
   var e = 0.0;
   var size = this.bodies.length;

   for (var i=0; i<size; i++) {
      var bodyi = this.bodies[i];

      e += 0.5 * bodyi.mass *
         ( bodyi.vx * bodyi.vx
         + bodyi.vy * bodyi.vy
         + bodyi.vz * bodyi.vz );

      for (var j=i+1; j<size; j++) {
         var bodyj = this.bodies[j];
         dx = bodyi.x - bodyj.x;
         dy = bodyi.y - bodyj.y;
         dz = bodyi.z - bodyj.z;

         distance = Math.sqrt(dx*dx + dy*dy + dz*dz);
         e -= (bodyi.mass * bodyj.mass) / distance;
      }
   }
   return e;
}

var ret = 0;

for ( var n = 3; n <= 24; n *= 2 ) {
    (function(){
        var bodies = new NBodySystem( Array(
           Sun(),Jupiter(),Saturn(),Uranus(),Neptune()
        ));
        var max = n * 100;
        
        ret += bodies.energy();
        for (var i=0; i<max; i++){
            bodies.advance(0.01);
        }
        ret += bodies.energy();
    })();
}

var expected = -1.3524862408537381;
if (ret != expected)
    throw "ERROR: bad result: expected " + expected + " but got " + ret;



var _sunSpiderInterval = new Date() - _sunSpiderStartDate;

WScript.Echo("### TIME:", _sunSpiderInterval, "ms");