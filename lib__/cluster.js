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
'use strict';var cov_7cfes3q6w=function(){var path='cluster.js',hash='33fbf5875f269ddc23c9361bbc1bd68a6d6f44a3',global=new Function('return this')(),gcv='__coverage__',coverageData={path:'cluster.js',statementMap:{'0':{start:{line:24,column:22},end:{line:24,column:74}},'1':{start:{line:25,column:0},end:{line:25,column:62}}},fnMap:{},branchMap:{'0':{loc:{start:{line:24,column:22},end:{line:24,column:74}},type:'cond-expr',locations:[{start:{line:24,column:56},end:{line:24,column:63}},{start:{line:24,column:66},end:{line:24,column:74}}],line:24}},s:{'0':0,'1':0},f:{},b:{'0':[0,0]},_coverageSchema:'332fd63041d2c1bcb487cc26dd0d5f7d97098a6c'},coverage=global[gcv]||(global[gcv]={});if(coverage[path]&&coverage[path].hash===hash){return coverage[path];}coverageData.hash=hash;return coverage[path]=coverageData;}();const childOrMaster=(cov_7cfes3q6w.s[0]++,'NODE_UNIQUE_ID'in process.env?(cov_7cfes3q6w.b[0][0]++,'child'):(cov_7cfes3q6w.b[0][1]++,'master'));cov_7cfes3q6w.s[1]++;module.exports=require(`internal/cluster/${childOrMaster}`);