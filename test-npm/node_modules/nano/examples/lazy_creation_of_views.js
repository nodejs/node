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

module.exports = function() {
var nano       = require('nano')('http://localhost:5984')
  , users      = nano.use('users')
  , VIEWS      = { by_twitter_id: 
    { "map": "function(doc) { emit(doc.twitter.screen_name, doc); }" } }
  ;

/*****************************************************************************
 * user.get()
 ****************************************************************************/
function user_get(id, callback){
  return users.get(id, callback);
}

/*****************************************************************************
 * user.new()
 ****************************************************************************/
function user_new(id, body, callback) {
  return users.insert(body, id, callback);
}

/*****************************************************************************
 * user.create()
 ****************************************************************************/
function create_users_database(email_address,secret,name,retries) {
  nano.db.create('users', function (e,b,h) {
    user_create(email_address,secret,name,retries+1);
  });
}

function user_create(email_address,secret,name,retries) {
  if (!retries) {
   retries = 0;
  }
  users.insert( {email_address: email_address, secret: secret, name: name}, secret,
    function (e,b,h) {
      if(e && e.message === 'no_db_file'  && retries < 1) {
        return create_users_database(email_address,secret,name,retries);
      }
      (function() { return ; })(e,b,h);
  });
}

/*****************************************************************************
 * user.find()
 ****************************************************************************/
 // 
 // some more logic needed
 // what if design document exists but view doesnt, we cant just overwrite it
 //
 // we need a way to fectch and build on
 // and thats the reason why im not doing this at 5am

function user_find(view, id, opts, tried, callback) {
  if(typeof tried === 'function') {
    callback = tried;
    tried = {tried:0, max_retries:2};
  }
  users.view('users', view, opts, function (e,b,h) {
    if(e) { 
      var current_view = VIEWS[view];
      if(!current_view) { 
        e.message = 'View is not available';
        return callback(e,b,h); 
      }
      if(tried.tried < tried.max_retries) {
        if(e.message === 'missing' || e.message === 'deleted') { // create design document
          var design_doc = {views: {}};
          design_doc.views[view] = current_view;
          return users.insert(design_doc, '_design/users', function () {
            tried.tried += 1;
            user_find(view,id,opts,tried,callback);
          });
        }
        if(e.message === 'missing_named_view') {
          users.get('_design/users', function (e,b,h) { // create view
            tried.tried += 1;
            if(e) { return user_find(view,id,opts,tried,callback); }
            b.views[view] = current_view;
            users.insert(b, '_design/users', function (e,b,h) {
              return user_find(view,id,opts,tried,callback);
            });
          });
          return;
        }
      }
      else { return callback(e,b,h); } 
    }
    return callback(null,b,h);
  });
}

function user_first(view, id, callback) {
  return user_find(view, id, {startkey: ('"' + id + '"'), limit: 1}, callback);
}

return { new: user_new
       , get: user_get
       , create: user_create
       , first: user_first
       };
};