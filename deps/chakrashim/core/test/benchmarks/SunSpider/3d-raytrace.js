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

/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

function createVector(x,y,z) {
    return new Array(x,y,z);
}

function sqrLengthVector(self) {
    return self[0] * self[0] + self[1] * self[1] + self[2] * self[2];
}

function lengthVector(self) {
    return Math.sqrt(self[0] * self[0] + self[1] * self[1] + self[2] * self[2]);
}

function addVector(self, v) {
    self[0] += v[0];
    self[1] += v[1];
    self[2] += v[2];
    return self;
}

function subVector(self, v) {
    self[0] -= v[0];
    self[1] -= v[1];
    self[2] -= v[2];
    return self;
}

function scaleVector(self, scale) {
    self[0] *= scale;
    self[1] *= scale;
    self[2] *= scale;
    return self;
}

function normaliseVector(self) {
    var len = Math.sqrt(self[0] * self[0] + self[1] * self[1] + self[2] * self[2]);
    self[0] /= len;
    self[1] /= len;
    self[2] /= len;
    return self;
}

function add(v1, v2) {
    return new Array(v1[0] + v2[0], v1[1] + v2[1], v1[2] + v2[2]);
}

function sub(v1, v2) {
    return new Array(v1[0] - v2[0], v1[1] - v2[1], v1[2] - v2[2]);
}

function scalev(v1, v2) {
    return new Array(v1[0] * v2[0], v1[1] * v2[1], v1[2] * v2[2]);
}

function dot(v1, v2) {
    return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
}

function scale(v, scale) {
    return [v[0] * scale, v[1] * scale, v[2] * scale];
}

function cross(v1, v2) {
    return [v1[1] * v2[2] - v1[2] * v2[1], 
            v1[2] * v2[0] - v1[0] * v2[2],
            v1[0] * v2[1] - v1[1] * v2[0]];

}

function normalise(v) {
    var len = lengthVector(v);
    return [v[0] / len, v[1] / len, v[2] / len];
}

function transformMatrix(self, v) {
    var vals = self;
    var x  = vals[0] * v[0] + vals[1] * v[1] + vals[2] * v[2] + vals[3];
    var y  = vals[4] * v[0] + vals[5] * v[1] + vals[6] * v[2] + vals[7];
    var z  = vals[8] * v[0] + vals[9] * v[1] + vals[10] * v[2] + vals[11];
    return [x, y, z];
}

function invertMatrix(self) {
    var temp = new Array(16);
    var tx = -self[3];
    var ty = -self[7];
    var tz = -self[11];
    for (h = 0; h < 3; h++) 
        for (v = 0; v < 3; v++) 
            temp[h + v * 4] = self[v + h * 4];
    for (i = 0; i < 11; i++)
        self[i] = temp[i];
    self[3] = tx * self[0] + ty * self[1] + tz * self[2];
    self[7] = tx * self[4] + ty * self[5] + tz * self[6];
    self[11] = tx * self[8] + ty * self[9] + tz * self[10];
    return self;
}


// Triangle intersection using barycentric coord method
function Triangle(p1, p2, p3) {
    var edge1 = sub(p3, p1);
    var edge2 = sub(p2, p1);
    var normal = cross(edge1, edge2);
    if (Math.abs(normal[0]) > Math.abs(normal[1]))
        if (Math.abs(normal[0]) > Math.abs(normal[2]))
            this.axis = 0; 
        else 
            this.axis = 2;
    else
        if (Math.abs(normal[1]) > Math.abs(normal[2])) 
            this.axis = 1;
        else 
            this.axis = 2;
    var u = (this.axis + 1) % 3;
    var v = (this.axis + 2) % 3;
    var u1 = edge1[u];
    var v1 = edge1[v];
    
    var u2 = edge2[u];
    var v2 = edge2[v];
    this.normal = normalise(normal);
    this.nu = normal[u] / normal[this.axis];
    this.nv = normal[v] / normal[this.axis];
    this.nd = dot(normal, p1) / normal[this.axis];
    var det = u1 * v2 - v1 * u2;
    this.eu = p1[u];
    this.ev = p1[v]; 
    this.nu1 = u1 / det;
    this.nv1 = -v1 / det;
    this.nu2 = v2 / det;
    this.nv2 = -u2 / det; 
    this.material = [0.7, 0.7, 0.7];
}

Triangle.prototype.intersect = function(orig, dir, near, far) {
    var u = (this.axis + 1) % 3;
    var v = (this.axis + 2) % 3;
    var d = dir[this.axis] + this.nu * dir[u] + this.nv * dir[v];
    var t = (this.nd - orig[this.axis] - this.nu * orig[u] - this.nv * orig[v]) / d;
    if (t < near || t > far)
        return null;
    var Pu = orig[u] + t * dir[u] - this.eu;
    var Pv = orig[v] + t * dir[v] - this.ev;
    var a2 = Pv * this.nu1 + Pu * this.nv1;
    if (a2 < 0) 
        return null;
    var a3 = Pu * this.nu2 + Pv * this.nv2;
    if (a3 < 0) 
        return null;

    if ((a2 + a3) > 1) 
        return null;
    return t;
}

function Scene(a_triangles) {
    this.triangles = a_triangles;
    this.lights = [];
    this.ambient = [0,0,0];
    this.background = [0.8,0.8,1];
}
var zero = new Array(0,0,0);

Scene.prototype.intersect = function(origin, dir, near, far) {
    var closest = null;
    for (i = 0; i < this.triangles.length; i++) {
        var triangle = this.triangles[i];   
        var d = triangle.intersect(origin, dir, near, far);
        if (d == null || d > far || d < near)
            continue;
        far = d;
        closest = triangle;
    }
    
    if (!closest)
        return [this.background[0],this.background[1],this.background[2]];
        
    var normal = closest.normal;
    var hit = add(origin, scale(dir, far)); 
    if (dot(dir, normal) > 0)
        normal = [-normal[0], -normal[1], -normal[2]];
    
    var colour = null;
    if (closest.shader) {
        colour = closest.shader(closest, hit, dir);
    } else {
        colour = closest.material;
    }
    
    // do reflection
    var reflected = null;
    if (colour.reflection > 0.001) {
        var reflection = addVector(scale(normal, -2*dot(dir, normal)), dir);
        reflected = this.intersect(hit, reflection, 0.0001, 1000000);
        if (colour.reflection >= 0.999999)
            return reflected;
    }
    
    var l = [this.ambient[0], this.ambient[1], this.ambient[2]];
    for (var i = 0; i < this.lights.length; i++) {
        var light = this.lights[i];
        var toLight = sub(light, hit);
        var distance = lengthVector(toLight);
        scaleVector(toLight, 1.0/distance);
        distance -= 0.0001;
        if (this.blocked(hit, toLight, distance))
            continue;
        var nl = dot(normal, toLight);
        if (nl > 0)
            addVector(l, scale(light.colour, nl));
    }
    l = scalev(l, colour);
    if (reflected) {
        l = addVector(scaleVector(l, 1 - colour.reflection), scaleVector(reflected, colour.reflection));
    }
    return l;
}

Scene.prototype.blocked = function(O, D, far) {
    var near = 0.0001;
    var closest = null;
    for (i = 0; i < this.triangles.length; i++) {
        var triangle = this.triangles[i];   
        var d = triangle.intersect(O, D, near, far);
        if (d == null || d > far || d < near)
            continue;
        return true;
    }
    
    return false;
}


// this camera code is from notes i made ages ago, it is from *somewhere* -- i cannot remember where
// that somewhere is
function Camera(origin, lookat, up) {
    var zaxis = normaliseVector(subVector(lookat, origin));
    var xaxis = normaliseVector(cross(up, zaxis));
    var yaxis = normaliseVector(cross(xaxis, subVector([0,0,0], zaxis)));
    var m = new Array(16);
    m[0] = xaxis[0]; m[1] = xaxis[1]; m[2] = xaxis[2];
    m[4] = yaxis[0]; m[5] = yaxis[1]; m[6] = yaxis[2];
    m[8] = zaxis[0]; m[9] = zaxis[1]; m[10] = zaxis[2];
    invertMatrix(m);
    m[3] = 0; m[7] = 0; m[11] = 0;
    this.origin = origin;
    this.directions = new Array(4);
    this.directions[0] = normalise([-0.7,  0.7, 1]);
    this.directions[1] = normalise([ 0.7,  0.7, 1]);
    this.directions[2] = normalise([ 0.7, -0.7, 1]);
    this.directions[3] = normalise([-0.7, -0.7, 1]);
    this.directions[0] = transformMatrix(m, this.directions[0]);
    this.directions[1] = transformMatrix(m, this.directions[1]);
    this.directions[2] = transformMatrix(m, this.directions[2]);
    this.directions[3] = transformMatrix(m, this.directions[3]);
}

Camera.prototype.generateRayPair = function(y) {
    rays = new Array(new Object(), new Object());
    rays[0].origin = this.origin;
    rays[1].origin = this.origin;
    rays[0].dir = addVector(scale(this.directions[0], y), scale(this.directions[3], 1 - y));
    rays[1].dir = addVector(scale(this.directions[1], y), scale(this.directions[2], 1 - y));
    return rays;
}

function renderRows(camera, scene, pixels, width, height, starty, stopy) {
    for (var y = starty; y < stopy; y++) {
        var rays = camera.generateRayPair(y / height);
        for (var x = 0; x < width; x++) {
            var xp = x / width;
            var origin = addVector(scale(rays[0].origin, xp), scale(rays[1].origin, 1 - xp));
            var dir = normaliseVector(addVector(scale(rays[0].dir, xp), scale(rays[1].dir, 1 - xp)));
            var l = scene.intersect(origin, dir);
            pixels[y][x] = l;
        }
    }
}

Camera.prototype.render = function(scene, pixels, width, height) {
    var cam = this;
    var row = 0;
    renderRows(cam, scene, pixels, width, height, 0, height);
}



function raytraceScene()
{
    var startDate = new Date().getTime();
    var numTriangles = 2 * 6;
    var triangles = new Array();//numTriangles);
    var tfl = createVector(-10,  10, -10);
    var tfr = createVector( 10,  10, -10);
    var tbl = createVector(-10,  10,  10);
    var tbr = createVector( 10,  10,  10);
    var bfl = createVector(-10, -10, -10);
    var bfr = createVector( 10, -10, -10);
    var bbl = createVector(-10, -10,  10);
    var bbr = createVector( 10, -10,  10);
    
    // cube!!!
    // front
    var i = 0;
    
    triangles[i++] = new Triangle(tfl, tfr, bfr);
    triangles[i++] = new Triangle(tfl, bfr, bfl);
    // back
    triangles[i++] = new Triangle(tbl, tbr, bbr);
    triangles[i++] = new Triangle(tbl, bbr, bbl);
    //        triangles[i-1].material = [0.7,0.2,0.2];
    //            triangles[i-1].material.reflection = 0.8;
    // left
    triangles[i++] = new Triangle(tbl, tfl, bbl);
    //            triangles[i-1].reflection = 0.6;
    triangles[i++] = new Triangle(tfl, bfl, bbl);
    //            triangles[i-1].reflection = 0.6;
    // right
    triangles[i++] = new Triangle(tbr, tfr, bbr);
    triangles[i++] = new Triangle(tfr, bfr, bbr);
    // top
    triangles[i++] = new Triangle(tbl, tbr, tfr);
    triangles[i++] = new Triangle(tbl, tfr, tfl);
    // bottom
    triangles[i++] = new Triangle(bbl, bbr, bfr);
    triangles[i++] = new Triangle(bbl, bfr, bfl);
    
    //Floor!!!!
    var green = createVector(0.0, 0.4, 0.0);
    var grey = createVector(0.4, 0.4, 0.4);
    grey.reflection = 1.0;
    var floorShader = function(tri, pos, view) {
        var x = ((pos[0]/32) % 2 + 2) % 2;
        var z = ((pos[2]/32 + 0.3) % 2 + 2) % 2;
        if (x < 1 != z < 1) {
            //in the real world we use the fresnel term...
            //    var angle = 1-dot(view, tri.normal);
            //   angle *= angle;
            //  angle *= angle;
            // angle *= angle;
            //grey.reflection = angle;
            return grey;
        } else 
            return green;
    }
    var ffl = createVector(-1000, -30, -1000);
    var ffr = createVector( 1000, -30, -1000);
    var fbl = createVector(-1000, -30,  1000);
    var fbr = createVector( 1000, -30,  1000);
    triangles[i++] = new Triangle(fbl, fbr, ffr);
    triangles[i-1].shader = floorShader;
    triangles[i++] = new Triangle(fbl, ffr, ffl);
    triangles[i-1].shader = floorShader;
    
    var _scene = new Scene(triangles);
    _scene.lights[0] = createVector(20, 38, -22);
    _scene.lights[0].colour = createVector(0.7, 0.3, 0.3);
    _scene.lights[1] = createVector(-23, 40, 17);
    _scene.lights[1].colour = createVector(0.7, 0.3, 0.3);
    _scene.lights[2] = createVector(23, 20, 17);
    _scene.lights[2].colour = createVector(0.7, 0.7, 0.7);
    _scene.ambient = createVector(0.1, 0.1, 0.1);
    //  _scene.background = createVector(0.7, 0.7, 1.0);
    
    var size = 30;
    var pixels = new Array();
    for (var y = 0; y < size; y++) {
        pixels[y] = new Array();
        for (var x = 0; x < size; x++) {
            pixels[y][x] = 0;
        }
    }

    var _camera = new Camera(createVector(-40, 40, 40), createVector(0, 0, 0), createVector(0, 1, 0));
    _camera.render(_scene, pixels, size, size);

    return pixels;
}

function arrayToCanvasCommands(pixels)
{
    var s = '<canvas id="renderCanvas" width="30px" height="30px"></canvas><scr' + 'ipt>\\nvar pixels = [';
    var size = 30;
    for (var y = 0; y < size; y++) {
        s += "[";
        for (var x = 0; x < size; x++) {
            s += "[" + pixels[y][x] + "],";
        }
        s+= "],";
    }
    s += '];\n    var canvas = document.getElementById("renderCanvas").getContext("2d");\n\
\n\
\n\
    var size = 30;\n\
    canvas.fillStyle = "red";\n\
    canvas.fillRect(0, 0, size, size);\n\
    canvas.scale(1, -1);\n\
    canvas.translate(0, -size);\n\
\n\
    if (!canvas.setFillColor)\n\
        canvas.setFillColor = function(r, g, b, a) {\n\
            this.fillStyle = "rgb("+[Math.floor(r * 255), Math.floor(g * 255), Math.floor(b * 255)]+")";\n\
    }\n\
\n\
for (var y = 0; y < size; y++) {\n\
  for (var x = 0; x < size; x++) {\n\
    var l = pixels[y][x];\n\
    canvas.setFillColor(l[0], l[1], l[2], 1);\n\
    canvas.fillRect(x, y, 1, 1);\n\
  }\n\
}</scr' + 'ipt>';

    return s;
}

testOutput = arrayToCanvasCommands(raytraceScene());

var _sunSpiderInterval = new Date() - _sunSpiderStartDate;

WScript.Echo("### TIME:", _sunSpiderInterval, "ms");