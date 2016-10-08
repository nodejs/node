# Installation

Install node.

Install npm.

<pre>
$ git clone repo
$ cd node.couchapp.js
$ npm install
$ npm link .
</pre>

<pre>
$ couchapp help
couchapp -- utility for creating couchapps

Usage - old style with single app.js:
  couchapp &lt;command> app.js http://localhost:5984/dbname [opts]

Usage - new style with multiple app files:
  directory based config specified by switch - multiple app files and pre- and post-processing capability)
  couchapp -dc &lt;<command> &lt;appconfigdirectory> http://localhost:5984/dbname

Commands:
  push   : Push app once to server.
  sync   : Push app then watch local files for changes.
  boiler : Create a boiler project.
  serve  : Serve couchapp from development webserver
            you can specify some options
            -p port  : list on port portNum [default=3000]
            -d dir   : attachments directory [default='attachments']
            -l       : log rewrites to couchdb [default='false']
</pre>

<pre>
Directory-based config:

  -dc (directory config) switch uses multiple config files in the directory specified by &lt;appconfigdirectory>

  Any file with a filename that begins with "app" will be executed.

  Additionally

  (i) if the app config directory contains file beforepushsync.js then this will be executed before any of the app files have run
  (ii) if the app config directory contains file afterpushsync.js then this will be executed after all of the app files have run

  beforepushsync.js and afterpushsync.js can be used to perform any before/after processing, using node.js code for example.

  The sample afterpushsync.js shows lookup data being added to CouchDB after the CouchApp has been pushed.
</pre>

app.js example:

<pre>
  var couchapp = require('couchapp')
    , path = require('path');

  ddoc = {
      _id: '_design/app'
    , views: {}
    , lists: {}
    , shows: {} 
  }

  module.exports = ddoc;

  ddoc.views.byType = {
    map: function(doc) {
      emit(doc.type, null);
    },
    reduce: '_count'
  }

  ddoc.views.peopleByName = {
    map: function(doc) {
      if(doc.type == 'person') {
        emit(doc.name, null);
      }
    }
  }

  ddoc.lists.people = function(head, req) {
    start({
      headers: {"Content-type": "text/html"}
    });
    send("&lt;ul id='people'>\n");
    while(row = getRow()) {
      send("\t&lt;li class='person name'>" + row.key + "&lt;/li>\n");
    }
    send("&lt;/ul>\n")
  }

  ddoc.shows.person = function(doc, req) {
    return {
      headers: {"Content-type": "text/html"},
      body: "&lt;h1 id='person' class='name'>" + doc.name + "&lt;/h1>\n"
    }
  }
  
  ddoc.validate_doc_update = function (newDoc, oldDoc, userCtx) {
    function require(field, message) {
      message = message || "Document must have a " + field;
      if (!newDoc[field]) throw({forbidden : message});
    };

    if (newDoc.type == "person") {
      require("name");
    }
  }

  couchapp.loadAttachments(ddoc, path.join(__dirname, '_attachments'));
</pre>


Local development server example.

Start the server:

    couchapp serve app.js http://localhost:5984/example_db -p 3000 -l -d attachments

Now you can access your couchapp at http://localhost:3000/ . Code, hack and when you are
happy with the result simply do:

    couchapp push app.js http://localhost:5984/example_db
