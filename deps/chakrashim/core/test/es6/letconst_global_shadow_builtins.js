//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function print(x) { WScript.Echo(x+'') }

print(this.Math);
// REVIEW: Should a let that shadows a built-in (or undeclared)
// cause Use Before Declaration errors at naked references?
// I would say yes.
//print(Math.PI);
//print(Math);

let Math = { PIE: "delicious" };

print(this.Math);
print(this.Math.PI);
print(this.Math.PIE);
print(Math);
print(Math.PI);
print(Math.PIE);

print(this.JSON);
//print(JSON.stringify);
//print(JSON);

const JSON = { stringifize: "no dice" };

print(this.JSON);
print(this.JSON.stringify);
print(this.JSON.stringifize);
print(JSON);
print(JSON.stringify);
print(JSON.stringifize);

// Test for bugfix 822845
//   Exprgen:CAS::CHK: ASSERT:  info->IsWritable() (jscript9test!Js::CacheOperators::CachePropertyWrite<1,Js::InlineCache> 811
let NaN;
NaN=1;
print(NaN);  // output:"NaN" desired:"1" pending:bug896007
print(this.NaN);

let Infinity;
Infinity=2;
print(Infinity);  // output:"Infinity" desired:"2" pending:bug896007
print(this.Infinity);

let undefined;
undefined=3;
print(undefined);  // output:"Infinity" desired:"2" pending:bug896007
print(this.undefined);

