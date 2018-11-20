// Copyright 2013 the V8 project authors. All rights reserved.
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

// Flags: --allow-natives-syntax

var gdpRatio = 16/9;

function Foo(initialX, initialY, initialScale, initialMapHeight) {
  this.ORIGIN = { x: initialX, y: initialY };
  this.scale = initialScale;
  this.mapHeight = initialMapHeight;
}

Foo.prototype.bar = function (x, y, xOffset, yOffset) {
  var tileHeight =  64 * 0.25 * this.scale,
  tileWidth  = 128 * 0.25 * this.scale,
  xOffset    = xOffset * 0.5 || 0,
  yOffset    = yOffset * 0.5 || 0;
  var xPos = ((xOffset) * gdpRatio) + this.ORIGIN.x * this.scale -
      ((y * tileWidth ) * gdpRatio) + ((x * tileWidth ) * gdpRatio);
  var yPos = ((yOffset) * gdpRatio) + this.ORIGIN.y * this.scale +
      ((y * tileHeight) * gdpRatio) + ((x * tileHeight) * gdpRatio);
  xPos = xPos - Math.round(((tileWidth) * gdpRatio)) +
      this.mapHeight * Math.round(((tileWidth) * gdpRatio));
  return {
      x: Math.floor(xPos),
      y: Math.floor(yPos)
  };
}

var f = new Foo(10, 20, 2.5, 400);

function baz() {
  var b = f.bar(1.1, 2.2, 3.3, 4.4);
  assertEquals(56529, b.x);
  assertEquals(288, b.y);
}

baz();
baz();
%OptimizeFunctionOnNextCall(Foo.prototype.bar);
baz();
