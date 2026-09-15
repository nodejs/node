var err = new Error('foo');
var date = new Date();
// commented out bits are either tested elsewhere or not supported yet. or uncloneable.
var tests = [undefined, null, false, true, 1, NaN, Infinity, 'foo', date, /foo/, /* ImageData, File, FileData, FileList,*/ null/*self*/,
              [undefined, null, false, true, 1, NaN, Infinity, 'foo', /*date, /foo/,*/ null/*self*/, /*[], {},*/ null/*err*/],
              {a:undefined, b:null, c:false, d:true, e:1, f:NaN, g:Infinity, h:'foo', /*i:date, j:/foo/,*/ k:null/*self*/, /*l:[], m:{},*/ n:null/*err*/},
            null/*err*/];
for (var i = 0; i < tests.length; ++i) {
  try {
    postMessage(tests[i]);
  } catch(e) {
    postMessage(''+e);
  }
}