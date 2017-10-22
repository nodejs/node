// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.
'use strict';// the sys module was renamed to 'util'.
// this shim remains to keep old programs working.
// sys is deprecated and shouldn't be used
var cov_1sdnmjzezp=function(){var path='sys.js',hash='36bf7060e3a87c2ab8193e79e3212ff4429b583e',global=new Function('return this')(),gcv='__coverage__',coverageData={path:'sys.js',statementMap:{'0':{start:{line:28,column:0},end:{line:28,column:33}},'1':{start:{line:29,column:0},end:{line:30,column:53}}},fnMap:{},branchMap:{},s:{'0':0,'1':0},f:{},b:{},_coverageSchema:'332fd63041d2c1bcb487cc26dd0d5f7d97098a6c'},coverage=global[gcv]||(global[gcv]={});if(coverage[path]&&coverage[path].hash===hash){return coverage[path];}coverageData.hash=hash;return coverage[path]=coverageData;}();cov_1sdnmjzezp.s[0]++;module.exports=require('util');cov_1sdnmjzezp.s[1]++;process.emitWarning('sys is deprecated. Use util instead.','DeprecationWarning','DEP0025');