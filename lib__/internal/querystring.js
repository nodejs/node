'use strict';var cov_1zk9wwums3=function(){var path='internal/querystring.js',hash='6d94d38dce01627c32abd60b8dfed242d6044f01',global=new Function('return this')(),gcv='__coverage__',coverageData={path:'internal/querystring.js',statementMap:{'0':{start:{line:3,column:17},end:{line:3,column:31}},'1':{start:{line:4,column:0},end:{line:5,column:75}},'2':{start:{line:5,column:2},end:{line:5,column:75}},'3':{start:{line:7,column:19},end:{line:24,column:1}},'4':{start:{line:26,column:0},end:{line:29,column:2}}},fnMap:{},branchMap:{'0':{loc:{start:{line:5,column:24},end:{line:5,column:41}},type:'cond-expr',locations:[{start:{line:5,column:33},end:{line:5,column:36}},{start:{line:5,column:39},end:{line:5,column:41}}],line:5}},s:{'0':0,'1':0,'2':0,'3':0,'4':0},f:{},b:{'0':[0,0]},_coverageSchema:'332fd63041d2c1bcb487cc26dd0d5f7d97098a6c'},coverage=global[gcv]||(global[gcv]={});if(coverage[path]&&coverage[path].hash===hash){return coverage[path];}coverageData.hash=hash;return coverage[path]=coverageData;}();const hexTable=(cov_1zk9wwums3.s[0]++,new Array(256));cov_1zk9wwums3.s[1]++;for(var i=0;i<256;++i){cov_1zk9wwums3.s[2]++;hexTable[i]='%'+((i<16?(cov_1zk9wwums3.b[0][0]++,'0'):(cov_1zk9wwums3.b[0][1]++,''))+i.toString(16)).toUpperCase();}const isHexTable=(cov_1zk9wwums3.s[3]++,[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,// 0 - 15
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,// 16 - 31
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,// 32 - 47
1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,// 48 - 63
0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,// 64 - 79
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,// 80 - 95
0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,// 96 - 111
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,// 112 - 127
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,// 128 ...
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0// ... 256
]);cov_1zk9wwums3.s[4]++;module.exports={hexTable,isHexTable};