[![By](https://img.shields.io/badge/made%20by-yld!-32bbee.svg?style=flat-square)](http://yld.io/contact?source=github-nano)[![Chat](https://img.shields.io/badge/help-gitter-eb9348.svg?style=flat-square)](https://gitter.im/dscape/nano)[![Tests](http://img.shields.io/travis/dscape/nano.svg?style=flat-square)](https://travis-ci.org/dscape/nano)![Coverage](https://img.shields.io/badge/coverage-100%-ff69b4.svg?style=flat-square)[![Dependencies](https://img.shields.io/david/dscape/nano.svg?style=flat-square)](https://david-dm.org/dscape/nano)[![NPM](http://img.shields.io/npm/v/nano.svg?style=flat-square)](http://browsenpm.org/package/nano)

# nano

minimalistic couchdb driver for node.js

`nano` features:

* **minimalistic** - there is only a minimum of abstraction between you and
  couchdb
* **pipes** - proxy requests from couchdb directly to your end user
* **errors** - errors are proxied directly from couchdb: if you know couchdb
  you already know `nano`.

## installation

1. install [npm][1]
2. `npm install nano`

## table of contents

- [getting started](#getting-started)
- [tutorials & screencasts](#tutorials-examples-in-the-wild--screencasts)
- [configuration](#configuration)
- [database functions](#database-functions)
  - [nano.db.create(name, [callback])](#nanodbcreatename-callback)
  - [nano.db.get(name, [callback])](#nanodbgetname-callback)
  - [nano.db.destroy(name, [callback])](#nanodbdestroyname-callback)
  - [nano.db.list([callback])](#nanodblistcallback)
  - [nano.db.compact(name, [designname], [callback])](#nanodbcompactname-designname-callback)
  - [nano.db.replicate(source, target, [opts], [callback])](#nanodbreplicatesource-target-opts-callback)
  - [nano.db.changes(name, [params], [callback])](#nanodbchangesname-params-callback)
  - [nano.db.follow(name, [params], [callback])](#nanodbfollowname-params-callback)
  - [nano.use(name)](#nanousename)
  - [nano.request(opts, [callback])](#nanorequestopts-callback)
  - [nano.config](#nanoconfig)
  - [nano.updates([params], [callback])](#nanoupdatesparams-callback)
  - [nano.followUpdates([params], [callback])](#nanofollowupdatesparams-callback)
- [document functions](#document-functions)
  - [db.insert(doc, [params], [callback])](#dbinsertdoc-params-callback)
  - [db.destroy(docname, rev, [callback])](#dbdestroydocname-rev-callback)
  - [db.get(docname, [params], [callback])](#dbgetdocname-params-callback)
  - [db.head(docname, [callback])](#dbheaddocname-callback)
  - [db.copy(src_doc, dest_doc, opts, [callback])](#dbcopysrc_doc-dest_doc-opts-callback)
  - [db.bulk(docs, [params], [callback])](#dbbulkdocs-params-callback)
  - [db.list([params], [callback])](#dblistparams-callback)
  - [db.fetch(docnames, [params], [callback])](#dbfetchdocnames-params-callback)
  - [db.fetchRevs(docnames, [params], [callback])](#dbfetchRevsdocnames-params-callback)
- [multipart functions](#multipart-functions)
  - [db.multipart.insert(doc, attachments, [params], [callback])](#dbmultipartinsertdoc-attachments-params-callback)
  - [db.multipart.get(docname, [params], [callback])](#dbmultipartgetdocname-params-callback)
- [attachments functions](#attachments-functions)
  - [db.attachment.insert(docname, attname, att, contenttype, [params], [callback])](#dbattachmentinsertdocname-attname-att-contenttype-params-callback)
  - [db.attachment.get(docname, attname, [params], [callback])](#dbattachmentgetdocname-attname-params-callback)
  - [db.attachment.destroy(docname, attname, [params], [callback])](#dbattachmentdestroydocname-attname-rev-callback)
- [views and design functions](#views-and-design-functions)
  - [db.view(designname, viewname, [params], [callback])](#dbviewdesignname-viewname-params-callback)
  - [db.show(designname, showname, doc_id, [params], [callback])](#dbshowdesignname-showname-doc_id-params-callback)
  - [db.atomic(designname, updatename, docname, [body], [callback])](#dbatomicdesignname-updatename-docname-body-callback)
  - [db.search(designname, viewname, [params], [callback])](#dbsearchdesignname-searchname-params-callback)
- [using cookie authentication](#using-cookie-authentication)
- [advanced features](#advanced-features)
  - [extending nano](#extending-nano)
  - [pipes](#pipes)
- [tests](#tests)

## getting started

to use `nano` you need to connect it to your couchdb install, to do that:

``` js
var nano = require('nano')('http://localhost:5984');
```

to create a new database:

``` js
nano.db.create('alice');
```

and to use it:

``` js
var alice = nano.db.use('alice');
```

in this examples we didn't specify a `callback` function, the absence of a
callback means _"do this, ignore what happens"_.
in `nano` the callback function receives always three arguments:

* `err` - the error, if any
* `body` - the http _response body_ from couchdb, if no error.
  json parsed body, binary for non json responses
* `header` - the http _response header_ from couchdb, if no error


a simple but complete example using callbacks is:

``` js
var nano = require('nano')('http://localhost:5984');

// clean up the database we created previously
nano.db.destroy('alice', function() {
  // create a new database
  nano.db.create('alice', function() {
    // specify the database we are going to use
    var alice = nano.use('alice');
    // and insert a document in it
    alice.insert({ crazy: true }, 'rabbit', function(err, body, header) {
      if (err) {
        console.log('[alice.insert] ', err.message);
        return;
      }
      console.log('you have inserted the rabbit.')
      console.log(body);
    });
  });
});
```

if you run this example(after starting couchdb) you will see:

    you have inserted the rabbit.
    { ok: true,
      id: 'rabbit',
      rev: '1-6e4cb465d49c0368ac3946506d26335d' }

you can also see your document in [futon](http://localhost:5984/_utils).

## configuration

configuring nano to use your database server is as simple as:

``` js
var nano   = require('nano')('http://localhost:5984')
  , db     = nano.use('foo')
  ;
```

however if you don't need to instrument database objects you can simply:

``` js
// nano parses the url and knows this is a database
var db = require('nano')('http://localhost:5984/foo');
```

you can also pass options to the require:

``` js
// nano parses the url and knows this is a database
var db = require('nano')('http://localhost:5984/foo');
```

to specify further configuration options you can pass an object literal instead:

``` js
// nano parses the url and knows this is a database
var db = require('nano')(
  { "url"             : "http://localhost:5984/foo"
  , "requestDefaults" : { "proxy" : "http://someproxy" }
  , "log"             : function (id, args) {
      console.log(id, args);
    }
  });
```
Please check [request] for more information on the defaults. They support features like cookie jar, proxies, ssl, etc.

You can tell nano to not parse the url (maybe the server is behind a proxy, is accessed through a rewrite rule or other):

```js
// nano does not parse the url and return the server api
// "http://localhost:5984/prefix" is the CouchDB server root
var couch = require('nano')(
  { "url"      : "http://localhost:5984/prefix"
    "parseUrl" : false
  });
var db = couch.use('foo');
```

### pool size and open sockets

a very important configuration parameter if you have a high traffic website and are using nano is setting up the `pool.size`. by default, the node.js http global agent (client) has a certain size of active connections that can run simultaneously, while others are kept in a queue. pooling can be disabled by setting the `agent` property in `requestDefaults` to false, or adjust the global pool size using:

``` js
http.globalAgent.maxSockets = 20;
```

you can also increase the size in your calling context using `requestDefaults` if this is problematic. refer to the [request] documentation and examples for further clarification.

here's an example explicitly using the keep alive agent (installed using `npm install agentkeepalive`), especially useful to limit your open sockets when doing high-volume access to couchdb on localhost:

``` js
var agentkeepalive = require('agentkeepalive');
var myagent = new agentkeepalive({
    maxSockets: 50
  , maxKeepAliveRequests: 0
  , maxKeepAliveTime: 30000
  });

var db = require('nano')(
  { "url"              : "http://localhost:5984/foo"
  , "requestDefaults" : { "agent" : myagent }
  });
```


## database functions

### nano.db.create(name, [callback])

creates a couchdb database with the given `name`.

``` js
nano.db.create('alice', function(err, body) {
  if (!err) {
    console.log('database alice created!');
  }
});
```

### nano.db.get(name, [callback])

get informations about `name`.

``` js
nano.db.get('alice', function(err, body) {
  if (!err) {
    console.log(body);
  }
});
```

### nano.db.destroy(name, [callback])

destroys `name`.

``` js
nano.db.destroy('alice');
```

even though this examples looks sync it is an async function.

### nano.db.list([callback])

lists all the databases in couchdb

``` js
nano.db.list(function(err, body) {
  // body is an array
  body.forEach(function(db) {
    console.log(db);
  });
});
```

### nano.db.compact(name, [designname], [callback])

compacts `name`, if `designname` is specified also compacts its
views.

### nano.db.replicate(source, target, [opts], [callback])

replicates `source` on `target` with options `opts`. `target`
has to exist, add `create_target:true` to `opts` to create it prior to
replication.

``` js
nano.db.replicate('alice', 'http://admin:password@otherhost.com:5984/alice',
                  { create_target:true }, function(err, body) {
    if (!err)
      console.log(body);
});
```

### nano.db.changes(name, [params], [callback])

asks for the changes feed of `name`, `params` contains additions
to the query string.

``` js
nano.db.changes('alice', function(err, body) {
  if (!err) {
    console.log(body);
  }
});
```

### nano.db.follow(name, [params], [callback])

Uses [Follow] to create a solid changes feed. please consult follow documentation for more information as this is a very complete API on it's own.

``` js
var feed = db.follow({since: "now"});
feed.on('change', function (change) {
  console.log("change: ", change);
});
feed.follow();
process.nextTick(function () {
  db.insert({"bar": "baz"}, "bar");
});
```

### nano.use(name)

creates a scope where you operate inside `name`.

``` js
var alice = nano.use('alice');
alice.insert({ crazy: true }, 'rabbit', function(err, body) {
  // do something
});
```

### nano.db.use(name)

alias for `nano.use`

### nano.db.scope(name)

alias for `nano.use`

### nano.scope(name)

alias for `nano.use`

### nano.request(opts, [callback])

makes a request to couchdb, the available `opts` are:

* `opts.db` – the database name
* `opts.method` – the http method, defaults to `get`
* `opts.path` – the full path of the request, overrides `opts.doc` and
  `opts.att`
* `opts.doc` – the document name
* `opts.att` – the attachment name
* `opts.qs` – query string parameters, appended after any existing `opts.path`, `opts.doc`, or `opts.att`
* `opts.content_type` – the content type of the request, default to `json`
* `opts.headers` – additional http headers, overrides existing ones
* `opts.body` – the document or attachment body
* `opts.encoding` – the encoding for attachments
* `opts.multipart` – array of objects for multipart request

### nano.relax(opts, [callback])

alias for `nano.request`

### nano.dinosaur(opts, [callback])

alias for `nano.request`

                    _
                  / '_)  WAT U SAY!
         _.----._/  /
        /          /
      _/  (   | ( |
     /__.-|_|--|_l

### nano.config

an object containing the nano configurations, possible keys are:

* `url` - the couchdb url
* `db` - the database name


### nano.updates([params], [callback])

listen to db updates, the available `params` are:

* `params.feed` – Type of feed. Can be one of
 * `longpoll`: Closes the connection after the first event.
 * `continuous`: Send a line of JSON per event. Keeps the socket open until timeout.
 * `eventsource`: Like, continuous, but sends the events in EventSource format.
* `params.timeout` – Number of seconds until CouchDB closes the connection. Default is 60.
* `params.heartbeat` – Whether CouchDB will send a newline character (\n) on timeout. Default is true.


### nano.followUpdates([params], [callback])

** changed in version 6 **

Use [Follow](https://github.com/jhs/follow) to create a solid
[`_db_updates`](http://docs.couchdb.org/en/latest/api/server/common.html?highlight=db_updates#get--_db_updates) feed.
Please consult follow documentation for more information as this is a very complete api on it's own

```js
var feed = nano.followUpdates({since: "now"});
feed.on('change', function (change) {
  console.log("change: ", change);
});
feed.follow();
process.nextTick(function () {
  nano.db.create('alice');
});
```

## document functions

### db.insert(doc, [params], [callback])

inserts `doc` in the database with  optional `params`. if params is a string, its assumed as the intended document name. if params is an object, its passed as query string parameters and `docName` is checked for defining the document name.

``` js
var alice = nano.use('alice');
alice.insert({ crazy: true }, 'rabbit', function(err, body) {
  if (!err)
    console.log(body);
});
```

The `insert` function can also be used with the method signature `db.insert(doc,[callback])`, where the `doc` contains the `_id` field e.g.

~~~ js
var alice = cloudant.use('alice')
alice.insert({ _id: 'myid', crazy: true }, function(err, body) {
  if (!err)
    console.log(body)
})
~~~

and also used to update an existing document, by including the `_rev` token in the document being saved:

~~~ js
var alice = cloudant.use('alice')
alice.insert({ _id: 'myid', _rev: '1-23202479633c2b380f79507a776743d5', crazy: false }, function(err, body) {
  if (!err)
    console.log(body)
})
~~~

### db.destroy(docname, rev, [callback])

removes revision `rev` of `docname` from couchdb.

``` js
alice.destroy('rabbit', '3-66c01cdf99e84c83a9b3fe65b88db8c0', function(err, body) {
  if (!err)
    console.log(body);
});
```

### db.get(docname, [params], [callback])

gets `docname` from the database with optional query string
additions `params`.

``` js
alice.get('rabbit', { revs_info: true }, function(err, body) {
  if (!err)
    console.log(body);
});
```

### db.head(docname, [callback])

same as `get` but lightweight version that returns headers only.

``` js
alice.head('rabbit', function(err, _, headers) {
  if (!err)
    console.log(headers);
});
```

### db.copy(src_doc, dest_doc, opts, [callback])

`copy` the contents (and attachments) of a document
to a new document, or overwrite an existing target document

``` js
alice.copy('rabbit', 'rabbit2', { overwrite: true }, function(err, _, headers) {
  if (!err)
    console.log(headers);
});
```


### db.bulk(docs, [params], [callback])

bulk operations(update/delete/insert) on the database, refer to the
[couchdb doc](http://wiki.apache.org/couchdb/HTTP_Bulk_Document_API).

### db.list([params], [callback])

list all the docs in the database with optional query string additions `params`.

``` js
alice.list(function(err, body) {
  if (!err) {
    body.rows.forEach(function(doc) {
      console.log(doc);
    });
  }
});
```

### db.fetch(docnames, [params], [callback])

bulk fetch of the database documents, `docnames` are specified as per
[couchdb doc](http://wiki.apache.org/couchdb/HTTP_Bulk_Document_API).
additional query string `params` can be specified, `include_docs` is always set
to `true`.

### db.fetchRevs(docnames, [params], [callback])

** changed in version 6 **

bulk fetch of the revisions of the database documents, `docnames` are specified as per
[couchdb doc](http://wiki.apache.org/couchdb/HTTP_Bulk_Document_API).
additional query string `params` can be specified, this is the same method as fetch but
 `include_docs` is not automatically set to `true`.

## multipart functions

### db.multipart.insert(doc, attachments, params, [callback])

inserts a `doc` together with `attachments` and `params`. if params is a string, its assumed as the intended document name. if params is an object, its passed as query string parameters and `docName` is checked for defining the document name.
 refer to the [doc](http://wiki.apache.org/couchdb/HTTP_Document_API#Multiple_Attachments) for more details.
 `attachments` must be an array of objects with `name`, `data` and `content_type` properties.

``` js
var fs = require('fs');

fs.readFile('rabbit.png', function(err, data) {
  if (!err) {
    alice.multipart.insert({ foo: 'bar' }, [{name: 'rabbit.png', data: data, content_type: 'image/png'}], 'mydoc', function(err, body) {
        if (!err)
          console.log(body);
    });
  }
});
```

### db.multipart.get(docname, [params], [callback])

get `docname` together with its attachments via `multipart/related` request with optional query string additions
`params`. refer to the
 [doc](http://wiki.apache.org/couchdb/HTTP_Document_API#Getting_Attachments_With_a_Document) for more details.
 the multipart response body is a `Buffer`.

``` js
alice.multipart.get('rabbit', function(err, buffer) {
  if (!err)
    console.log(buffer.toString());
});
```

## attachments functions

### db.attachment.insert(docname, attname, att, contenttype, [params], [callback])

inserts an attachment `attname` to `docname`, in most cases
 `params.rev` is required. refer to the
 [doc](http://wiki.apache.org/couchdb/HTTP_Document_API) for more details.

``` js
var fs = require('fs');

fs.readFile('rabbit.png', function(err, data) {
  if (!err) {
    alice.attachment.insert('rabbit', 'rabbit.png', data, 'image/png',
      { rev: '12-150985a725ec88be471921a54ce91452' }, function(err, body) {
        if (!err)
          console.log(body);
    });
  }
});
```

or using `pipe`:

``` js
var fs = require('fs');

fs.createReadStream('rabbit.png').pipe(
    alice.attachment.insert('new', 'rab.png', null, 'image/png')
);
```

### db.attachment.get(docname, attname, [params], [callback])

get `docname`'s attachment `attname` with optional query string additions
`params`.

``` js
var fs = require('fs');

alice.attachment.get('rabbit', 'rabbit.png', function(err, body) {
  if (!err) {
    fs.writeFile('rabbit.png', body);
  }
});
```

or using `pipe`:

``` js
var fs = require('fs');

alice.attachment.get('rabbit', 'rabbit.png').pipe(fs.createWriteStream('rabbit.png'));
```

### db.attachment.destroy(docname, attname, [params], [callback])

**changed in version 6**

destroy attachment `attname` of `docname`'s revision `rev`.

``` js
alice.attachment.destroy('rabbit', 'rabbit.png',
    {rev: '1-4701d73a08ce5c2f2983bf7c9ffd3320'}, function(err, body) {
      if (!err)
        console.log(body);
});
```

## views and design functions

### db.view(designname, viewname, [params], [callback])

calls a view of the specified design with optional query string additions
`params`. if you're looking to filter the view results by key(s) pass an array of keys, e.g
`{ keys: ['key1', 'key2', 'key_n'] }`, as `params`.

``` js
alice.view('characters', 'crazy_ones', function(err, body) {
  if (!err) {
    body.rows.forEach(function(doc) {
      console.log(doc.value);
    });
  }
});
```

### db.viewWithList(designname, viewname, listname, [params], [callback])

calls a list function feeded by the given view of the specified design document.

``` js
alice.viewWithList('characters', 'crazy_ones', 'my_list', function(err, body) {
  if (!err) {
    console.log(body);
  }
});
```

### db.show(designname, showname, doc_id, [params], [callback])

calls a show function of the specified design for the document specified by doc_id with
optional query string additions `params`.

``` js
alice.show('characters', 'format_doc', '3621898430', function(err, doc) {
  if (!err) {
    console.log(doc);
  }
});
```
take a look at the [couchdb wiki](http://wiki.apache.org/couchdb/Formatting_with_Show_and_List#Showing_Documents)
for possible query paramaters and more information on show functions.

### db.atomic(designname, updatename, docname, [body], [callback])

calls the design's update function with the specified doc in input.

``` js
db.atomic("update", "inplace", "foobar",
{field: "foo", value: "bar"}, function (error, response) {
  assert.equal(error, undefined, "failed to update");
  assert.equal(response.foo, "bar", "update worked");
});
```

Note that the data is sent in the body of the request.
An example update handler follows:

``` js
"updates": {
  "in-place" : "function(doc, req) {
      var field = req.form.field;
      var value = req.form.value;
      var message = 'set '+field+' to '+value;
      doc[field] = value;
      return [doc, message];
  }"
```

### db.search(designname, searchname, [params], [callback])

calls a view of the specified design with optional query string additions `params`.

``` js
alice.search('characters', 'crazy_ones', { q: 'cat' }, function(err, doc) {
  if (!err) {
    console.log(doc);
  }
});
```

check out the tests for a fully functioning example.

## using cookie authentication

nano supports making requests using couchdb's [cookie authentication](http://guide.couchdb.org/editions/1/en/security.html#cookies) functionality. there's a [example in coffeescript](http://stackoverflow.com/questions/23100132/using-nano-auth-correctly), but essentially you just:

``` js
var nano     = require('nano')('http://localhost:5984')
  , username = 'user'
  , userpass = 'pass'
  , callback = console.log // this would normally be some callback
  , cookies  = {} // store cookies, normally redis or something
  ;

nano.auth(username, userpass, function (err, body, headers) {
  if (err) {
    return callback(err);
  }

  if (headers && headers['set-cookie']) {
    cookies[user] = headers['set-cookie'];
  }

  callback(null, "it worked");
});
```

reusing a cookie:

``` js
var auth = "some stored cookie"
  , callback = console.log // this would normally be some callback
  , alice = require('nano')(
    { url : 'http://localhost:5984/alice', cookie: 'AuthSession=' + auth });
  ;

alice.insert(doc, function (err, body, headers) {
  if (err) {
    return callback(err);
  }

  // change the cookie if couchdb tells us to
  if (headers && headers['set-cookie']) {
    auth = headers['set-cookie'];
  }

  callback(null, "it worked");
});
```

getting current session:

```javascript
var nano = require('nano')({url: 'http://localhost:5984', cookie: 'AuthSession=' + auth});

nano.session(function(err, session) {
  if (err) {
    return console.log('oh noes!')
  }

  console.log('user is %s and has these roles: %j',
    session.userCtx.name, session.userCtx.roles);
});
```


## advanced features

### extending nano

nano is minimalistic but you can add your own features with
`nano.request(opts, callback)`

for example, to create a function to retrieve a specific revision of the
`rabbit` document:

``` js
function getrabbitrev(rev, callback) {
  nano.request({ db: 'alice',
                 doc: 'rabbit',
                 method: 'get',
                 params: { rev: rev }
               }, callback);
}

getrabbitrev('4-2e6cdc4c7e26b745c2881a24e0eeece2', function(err, body) {
  if (!err) {
    console.log(body);
  }
});
```
### pipes

you can pipe in nano like in any other stream.
for example if our `rabbit` document has an attachment with name `picture.png`
(with a picture of our white rabbit, of course!) you can pipe it to a `writable
stream`

``` js
var fs = require('fs'),
    nano = require('nano')('http://127.0.0.1:5984/');
var alice = nano.use('alice');
alice.attachment.get('rabbit', 'picture.png').pipe(fs.createWriteStream('/tmp/rabbit.png'));
```

then open `/tmp/rabbit.png` and you will see the rabbit picture.


## tutorials, examples in the wild & screencasts

* article: [nano - a minimalistic couchdb client for nodejs](http://writings.nunojob.com/2011/08/nano-minimalistic-couchdb-client-for-nodejs.html)
* article: [getting started with node.js and couchdb](http://writings.nunojob.com/2011/09/getting-started-with-nodejs-and-couchdb.html)
* article: [document update handler support](http://jackhq.tumblr.com/post/16035106690/nano-v1-2-x-document-update-handler-support-v1-2-x)
* article: [nano 3](http://writings.nunojob.com/2012/05/Nano-3.html)
* article: [securing a site with couchdb cookie authentication using node.js and nano](http://codetwizzle.com/articles/couchdb-cookie-authentication-nodejs-nano/)
* article: [adding copy to nano](http://blog.jlank.com/2012/07/04/adding-copy-to-nano/)
* article: [how to update a document with nano](http://writings.nunojob.com/2012/07/How-To-Update-A-Document-With-Nano-The-CouchDB-Client-for-Node.js.html)
* article: [thoughts on development using couchdb with node.js](http://tbranyen.com/post/thoughts-on-development-using-couchdb-with-nodejs)
* example in the wild: [nanoblog](https://github.com/grabbeh/nanoblog)

## roadmap

check [issues][2]

## tests

to run (and configure) the test suite simply:

``` sh
cd nano
npm install
npm test
```

after adding a new test you can run it individually (with verbose output) using:

``` sh
nano_env=testing node tests/doc/list.js list_doc_params
```

where `list_doc_params` is the test name.

## meta

                    _
                  / _) roar! i'm a vegan!
           .-^^^-/ /
        __/       /
       /__.|_|-|_|     cannes est superb

* code: `git clone git://github.com/dscape/nano.git`
* home: <http://github.com/dscape/nano>
* bugs: <http://github.com/dscape/nano/issues>
* build: [![build status](https://secure.travis-ci.org/dscape/nano.png)](http://travis-ci.org/dscape/nano)
* deps: [![deps status](https://david-dm.org/dscape/nano.png)](https://david-dm.org/dscape/nano)
* chat: <https://gitter.im/dscape/nano>

`(oo)--',-` in [caos][3]

[1]: http://npmjs.org
[2]: http://github.com/dscape/nano/issues
[3]: http://caos.di.uminho.pt/
[4]: https://github.com/dscape/nano/blob/master/cfg/couch.example.js
[follow]: https://github.com/jhs/follow
[request]:  https://github.com/request/request

## license

copyright 2011 nuno job <nunojob.com> (oo)--',--

licensed under the apache license, version 2.0 (the "license");
you may not use this file except in compliance with the license.
you may obtain a copy of the license at

    http://www.apache.org/licenses/LICENSE-2.0.html

unless required by applicable law or agreed to in writing, software
distributed under the license is distributed on an "as is" basis,
without warranties or conditions of any kind, either express or implied.
see the license for the specific language governing permissions and
limitations under the license.
