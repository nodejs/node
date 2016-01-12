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

// 3D Cube Rotation
// http://www.speich.net/computer/moztesting/3d.htm
// Created by Simon Speich

var Q = new Array();
var MTrans = new Array();  // transformation matrix
var MQube = new Array();  // position information of qube
var I = new Array();      // entity matrix
var Origin = new Object();
var Testing = new Object();
var LoopTimer;

var validation = {
 20: 2889.0000000000045,
 40: 2889.0000000000055,
 80: 2889.000000000005,
 160: 2889.0000000000055
};

var DisplArea = new Object();
DisplArea.Width = 300;
DisplArea.Height = 300;

function DrawLine(From, To) {
  var x1 = From.V[0];
  var x2 = To.V[0];
  var y1 = From.V[1];
  var y2 = To.V[1];
  var dx = Math.abs(x2 - x1);
  var dy = Math.abs(y2 - y1);
  var x = x1;
  var y = y1;
  var IncX1, IncY1;
  var IncX2, IncY2;  
  var Den;
  var Num;
  var NumAdd;
  var NumPix;

  if (x2 >= x1) {  IncX1 = 1; IncX2 = 1;  }
  else { IncX1 = -1; IncX2 = -1; }
  if (y2 >= y1)  {  IncY1 = 1; IncY2 = 1; }
  else { IncY1 = -1; IncY2 = -1; }
  if (dx >= dy) {
    IncX1 = 0;
    IncY2 = 0;
    Den = dx;
    Num = dx / 2;
    NumAdd = dy;
    NumPix = dx;
  }
  else {
    IncX2 = 0;
    IncY1 = 0;
    Den = dy;
    Num = dy / 2;
    NumAdd = dx;
    NumPix = dy;
  }

  NumPix = Math.round(Q.LastPx + NumPix);

  var i = Q.LastPx;
  for (; i < NumPix; i++) {
    Num += NumAdd;
    if (Num >= Den) {
      Num -= Den;
      x += IncX1;
      y += IncY1;
    }
    x += IncX2;
    y += IncY2;
  }
  Q.LastPx = NumPix;
}

function CalcCross(V0, V1) {
  var Cross = new Array();
  Cross[0] = V0[1]*V1[2] - V0[2]*V1[1];
  Cross[1] = V0[2]*V1[0] - V0[0]*V1[2];
  Cross[2] = V0[0]*V1[1] - V0[1]*V1[0];
  return Cross;
}

function CalcNormal(V0, V1, V2) {
  var A = new Array();   var B = new Array(); 
  for (var i = 0; i < 3; i++) {
    A[i] = V0[i] - V1[i];
    B[i] = V2[i] - V1[i];
  }
  A = CalcCross(A, B);
  var Length = Math.sqrt(A[0]*A[0] + A[1]*A[1] + A[2]*A[2]); 
  for (var i = 0; i < 3; i++) A[i] = A[i] / Length;
  A[3] = 1;
  return A;
}

function CreateP(X,Y,Z) {
  this.V = [X,Y,Z,1];
}

// multiplies two matrices
function MMulti(M1, M2) {
  var M = [[],[],[],[]];
  var i = 0;
  var j = 0;
  for (; i < 4; i++) {
    j = 0;
    for (; j < 4; j++) M[i][j] = M1[i][0] * M2[0][j] + M1[i][1] * M2[1][j] + M1[i][2] * M2[2][j] + M1[i][3] * M2[3][j];
  }
  return M;
}

//multiplies matrix with vector
function VMulti(M, V) {
  var Vect = new Array();
  var i = 0;
  for (;i < 4; i++) Vect[i] = M[i][0] * V[0] + M[i][1] * V[1] + M[i][2] * V[2] + M[i][3] * V[3];
  return Vect;
}

function VMulti2(M, V) {
  var Vect = new Array();
  var i = 0;
  for (;i < 3; i++) Vect[i] = M[i][0] * V[0] + M[i][1] * V[1] + M[i][2] * V[2];
  return Vect;
}

// add to matrices
function MAdd(M1, M2) {
  var M = [[],[],[],[]];
  var i = 0;
  var j = 0;
  for (; i < 4; i++) {
    j = 0;
    for (; j < 4; j++) M[i][j] = M1[i][j] + M2[i][j];
  }
  return M;
}

function Translate(M, Dx, Dy, Dz) {
  var T = [
  [1,0,0,Dx],
  [0,1,0,Dy],
  [0,0,1,Dz],
  [0,0,0,1]
  ];
  return MMulti(T, M);
}

function RotateX(M, Phi) {
  var a = Phi;
  a *= Math.PI / 180;
  var Cos = Math.cos(a);
  var Sin = Math.sin(a);
  var R = [
  [1,0,0,0],
  [0,Cos,-Sin,0],
  [0,Sin,Cos,0],
  [0,0,0,1]
  ];
  return MMulti(R, M);
}

function RotateY(M, Phi) {
  var a = Phi;
  a *= Math.PI / 180;
  var Cos = Math.cos(a);
  var Sin = Math.sin(a);
  var R = [
  [Cos,0,Sin,0],
  [0,1,0,0],
  [-Sin,0,Cos,0],
  [0,0,0,1]
  ];
  return MMulti(R, M);
}

function RotateZ(M, Phi) {
  var a = Phi;
  a *= Math.PI / 180;
  var Cos = Math.cos(a);
  var Sin = Math.sin(a);
  var R = [
  [Cos,-Sin,0,0],
  [Sin,Cos,0,0],
  [0,0,1,0],   
  [0,0,0,1]
  ];
  return MMulti(R, M);
}

function DrawQube() {
  // calc current normals
  var CurN = new Array();
  var i = 5;
  Q.LastPx = 0;
  for (; i > -1; i--) CurN[i] = VMulti2(MQube, Q.Normal[i]);
  if (CurN[0][2] < 0) {
    if (!Q.Line[0]) { DrawLine(Q[0], Q[1]); Q.Line[0] = true; };
    if (!Q.Line[1]) { DrawLine(Q[1], Q[2]); Q.Line[1] = true; };
    if (!Q.Line[2]) { DrawLine(Q[2], Q[3]); Q.Line[2] = true; };
    if (!Q.Line[3]) { DrawLine(Q[3], Q[0]); Q.Line[3] = true; };
  }
  if (CurN[1][2] < 0) {
    if (!Q.Line[2]) { DrawLine(Q[3], Q[2]); Q.Line[2] = true; };
    if (!Q.Line[9]) { DrawLine(Q[2], Q[6]); Q.Line[9] = true; };
    if (!Q.Line[6]) { DrawLine(Q[6], Q[7]); Q.Line[6] = true; };
    if (!Q.Line[10]) { DrawLine(Q[7], Q[3]); Q.Line[10] = true; };
  }
  if (CurN[2][2] < 0) {
    if (!Q.Line[4]) { DrawLine(Q[4], Q[5]); Q.Line[4] = true; };
    if (!Q.Line[5]) { DrawLine(Q[5], Q[6]); Q.Line[5] = true; };
    if (!Q.Line[6]) { DrawLine(Q[6], Q[7]); Q.Line[6] = true; };
    if (!Q.Line[7]) { DrawLine(Q[7], Q[4]); Q.Line[7] = true; };
  }
  if (CurN[3][2] < 0) {
    if (!Q.Line[4]) { DrawLine(Q[4], Q[5]); Q.Line[4] = true; };
    if (!Q.Line[8]) { DrawLine(Q[5], Q[1]); Q.Line[8] = true; };
    if (!Q.Line[0]) { DrawLine(Q[1], Q[0]); Q.Line[0] = true; };
    if (!Q.Line[11]) { DrawLine(Q[0], Q[4]); Q.Line[11] = true; };
  }
  if (CurN[4][2] < 0) {
    if (!Q.Line[11]) { DrawLine(Q[4], Q[0]); Q.Line[11] = true; };
    if (!Q.Line[3]) { DrawLine(Q[0], Q[3]); Q.Line[3] = true; };
    if (!Q.Line[10]) { DrawLine(Q[3], Q[7]); Q.Line[10] = true; };
    if (!Q.Line[7]) { DrawLine(Q[7], Q[4]); Q.Line[7] = true; };
  }
  if (CurN[5][2] < 0) {
    if (!Q.Line[8]) { DrawLine(Q[1], Q[5]); Q.Line[8] = true; };
    if (!Q.Line[5]) { DrawLine(Q[5], Q[6]); Q.Line[5] = true; };
    if (!Q.Line[9]) { DrawLine(Q[6], Q[2]); Q.Line[9] = true; };
    if (!Q.Line[1]) { DrawLine(Q[2], Q[1]); Q.Line[1] = true; };
  }
  Q.Line = [false,false,false,false,false,false,false,false,false,false,false,false];
  Q.LastPx = 0;
}

function Loop() {
  if (Testing.LoopCount > Testing.LoopMax) return;
  var TestingStr = String(Testing.LoopCount);
  while (TestingStr.length < 3) TestingStr = "0" + TestingStr;
  MTrans = Translate(I, -Q[8].V[0], -Q[8].V[1], -Q[8].V[2]);
  MTrans = RotateX(MTrans, 1);
  MTrans = RotateY(MTrans, 3);
  MTrans = RotateZ(MTrans, 5);
  MTrans = Translate(MTrans, Q[8].V[0], Q[8].V[1], Q[8].V[2]);
  MQube = MMulti(MTrans, MQube);
  var i = 8;
  for (; i > -1; i--) {
    Q[i].V = VMulti(MTrans, Q[i].V);
  }
  DrawQube();
  Testing.LoopCount++;
  Loop();
}

function Init(CubeSize) {
  // init/reset vars
  Origin.V = [150,150,20,1];
  Testing.LoopCount = 0;
  Testing.LoopMax = 50;
  Testing.TimeMax = 0;
  Testing.TimeAvg = 0;
  Testing.TimeMin = 0;
  Testing.TimeTemp = 0;
  Testing.TimeTotal = 0;
  Testing.Init = false;

  // transformation matrix
  MTrans = [
  [1,0,0,0],
  [0,1,0,0],
  [0,0,1,0],
  [0,0,0,1]
  ];
  
  // position information of qube
  MQube = [
  [1,0,0,0],
  [0,1,0,0],
  [0,0,1,0],
  [0,0,0,1]
  ];
  
  // entity matrix
  I = [
  [1,0,0,0],
  [0,1,0,0],
  [0,0,1,0],
  [0,0,0,1]
  ];
  
  // create qube
  Q[0] = new CreateP(-CubeSize,-CubeSize, CubeSize);
  Q[1] = new CreateP(-CubeSize, CubeSize, CubeSize);
  Q[2] = new CreateP( CubeSize, CubeSize, CubeSize);
  Q[3] = new CreateP( CubeSize,-CubeSize, CubeSize);
  Q[4] = new CreateP(-CubeSize,-CubeSize,-CubeSize);
  Q[5] = new CreateP(-CubeSize, CubeSize,-CubeSize);
  Q[6] = new CreateP( CubeSize, CubeSize,-CubeSize);
  Q[7] = new CreateP( CubeSize,-CubeSize,-CubeSize);
  
  // center of gravity
  Q[8] = new CreateP(0, 0, 0);
  
  // anti-clockwise edge check
  Q.Edge = [[0,1,2],[3,2,6],[7,6,5],[4,5,1],[4,0,3],[1,5,6]];
  
  // calculate squad normals
  Q.Normal = new Array();
  for (var i = 0; i < Q.Edge.length; i++) Q.Normal[i] = CalcNormal(Q[Q.Edge[i][0]].V, Q[Q.Edge[i][1]].V, Q[Q.Edge[i][2]].V);
  
  // line drawn ?
  Q.Line = [false,false,false,false,false,false,false,false,false,false,false,false];
  
  // create line pixels
  Q.NumPx = 9 * 2 * CubeSize;
  for (var i = 0; i < Q.NumPx; i++) CreateP(0,0,0);
  
  MTrans = Translate(MTrans, Origin.V[0], Origin.V[1], Origin.V[2]);
  MQube = MMulti(MTrans, MQube);

  var i = 0;
  for (; i < 9; i++) {
    Q[i].V = VMulti(MTrans, Q[i].V);
  }
  DrawQube();
  Testing.Init = true;
  Loop();
  
  // Perform a simple sum-based verification.
  var sum = 0;
  for (var i = 0; i < Q.length; ++i) {
    var vector = Q[i].V;
    for (var j = 0; j < vector.length; ++j)
      sum += vector[j];
  }
  if (sum != validation[CubeSize])
    throw "Error: bad vector sum for CubeSize = " + CubeSize + "; expected " + validation[CubeSize] + " but got " + sum;
}

for ( var i = 20; i <= 160; i *= 2 ) {
  Init(i);
}

Q = null;
MTrans = null;
MQube = null;
I = null;
Origin = null;
Testing = null;
LoopTime = null;
DisplArea = null;

var _sunSpiderInterval = new Date() - _sunSpiderStartDate;

WScript.Echo("### TIME:", _sunSpiderInterval, "ms");
