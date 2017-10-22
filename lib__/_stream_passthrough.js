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
// a passthrough stream.
// basically just the most minimal sort of Transform stream.
// Every written chunk gets output as-is.
'use strict';var cov_1vlf6r5wcr=function(){var path='_stream_passthrough.js',hash='4e12e81c2adce84f8f44d9ff6361f74ed2c0c0c7',global=new Function('return this')(),gcv='__coverage__',coverageData={path:'_stream_passthrough.js',statementMap:{'0':{start:{line:28,column:0},end:{line:28,column:29}},'1':{start:{line:30,column:18},end:{line:30,column:46}},'2':{start:{line:31,column:13},end:{line:31,column:28}},'3':{start:{line:32,column:0},end:{line:32,column:38}},'4':{start:{line:35,column:2},end:{line:36,column:36}},'5':{start:{line:36,column:4},end:{line:36,column:36}},'6':{start:{line:38,column:2},end:{line:38,column:32}},'7':{start:{line:41,column:0},end:{line:43,column:2}},'8':{start:{line:42,column:2},end:{line:42,column:18}}},fnMap:{'0':{name:'PassThrough',decl:{start:{line:34,column:9},end:{line:34,column:20}},loc:{start:{line:34,column:30},end:{line:39,column:1}},line:34},'1':{name:'(anonymous_1)',decl:{start:{line:41,column:35},end:{line:41,column:36}},loc:{start:{line:41,column:65},end:{line:43,column:1}},line:41}},branchMap:{'0':{loc:{start:{line:35,column:2},end:{line:36,column:36}},type:'if',locations:[{start:{line:35,column:2},end:{line:36,column:36}},{start:{line:35,column:2},end:{line:36,column:36}}],line:35}},s:{'0':0,'1':0,'2':0,'3':0,'4':0,'5':0,'6':0,'7':0,'8':0},f:{'0':0,'1':0},b:{'0':[0,0]},_coverageSchema:'332fd63041d2c1bcb487cc26dd0d5f7d97098a6c'},coverage=global[gcv]||(global[gcv]={});if(coverage[path]&&coverage[path].hash===hash){return coverage[path];}coverageData.hash=hash;return coverage[path]=coverageData;}();cov_1vlf6r5wcr.s[0]++;module.exports=PassThrough;const Transform=(cov_1vlf6r5wcr.s[1]++,require('_stream_transform'));const util=(cov_1vlf6r5wcr.s[2]++,require('util'));cov_1vlf6r5wcr.s[3]++;util.inherits(PassThrough,Transform);function PassThrough(options){cov_1vlf6r5wcr.f[0]++;cov_1vlf6r5wcr.s[4]++;if(!(this instanceof PassThrough)){cov_1vlf6r5wcr.b[0][0]++;cov_1vlf6r5wcr.s[5]++;return new PassThrough(options);}else{cov_1vlf6r5wcr.b[0][1]++;}cov_1vlf6r5wcr.s[6]++;Transform.call(this,options);}cov_1vlf6r5wcr.s[7]++;PassThrough.prototype._transform=function(chunk,encoding,cb){cov_1vlf6r5wcr.f[1]++;cov_1vlf6r5wcr.s[8]++;cb(null,chunk);};