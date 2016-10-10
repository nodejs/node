var couchapp = require('../../main')
  , path = require('path')
  ;

ddoc = {_id: "_design/app_2", views: {}}
module.exports = ddoc;

ddoc.views.simple = {
  map: function (doc) {emit(doc._id, null)}
}
ddoc.views.simpleReduce = {
  map: function (doc) {emit(doc._id, 1)},
  reduce: function (keys, values, rereduce) {
    return sum(values);
  }
}
ddoc.modules = {}
// couchapp.addModuleTree(ddoc.modules, 'modules');

ddoc.tests = {views:{
  simple:{map:{expect:[ [{'_id':'1'}, {'_id':'2'}], [ ['1',null], ['2',null] ] ]}},
  simpleReduce:{map:{expect:[ [{'_id':'1'}, {'_id':'2'}], [ ['1',1], ['2',1] ] ]},
                reduce:{expect:[ [{'_id':'1'}, {'_id':'2'}], 2 ] }
}}}

couchapp.loadAttachments(ddoc, path.join(__dirname, 'modules'))