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

var express = require('express')
  , nano    = require('nano')('http://localhost:5984')
  , app     = module.exports = express.createServer()
  , db_name = "test"
  , db      = nano.use(db_name);

app.get("/", function(request,response) {
  db.attachment.get("new", "logo.png").pipe(response);
});

app.listen(3333);