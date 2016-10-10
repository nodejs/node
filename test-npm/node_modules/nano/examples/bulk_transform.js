// Licensed under the Apache License, Version 2.0 (the 'License'); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an 'AS IS' BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

var db    = require('nano')('http://localhost:5984/emails')
  , async = require('async')
  ;

function update_row(row,cb) {
  var doc = row.doc;
  delete doc.subject;
  db.insert(doc, doc._id, function (err, data) {
    if(err)  { console.log('err at ' + doc._id);  cb(err); }
    else     { console.log('updated ' + doc._id); cb(); }
  });
}

function list(offset) {
  var ended = false;
  offset = offset || 0;
  db.list({include_docs: true, limit: 10, skip: offset}, 
    function(err, data) {
      var total, offset, rows;
      if(err) { console.log('fuuuu: ' + err.message); rows = []; return; }
      total  = data.total_rows;
      offset = data.offset;
      rows   = data.rows;
      if(offset === total) { 
        ended = true;
        return; 
      }
      async.forEach(rows, update_row, function (err) {
        if(err) { console.log('something failed, check logs'); }
        if(ended) { return; }
        list(offset+10);
      });
  });
}

list();