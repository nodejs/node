# Follow: CouchDB changes and db updates notifier for NodeJS

[![build
status](https://secure.travis-ci.org/iriscouch/follow.png)](http://travis-ci.org/iriscouch/follow)

Follow (upper-case *F*) comes from an internal Iris Couch project used in production for over a year. It works in the browser (beta) and is available as an NPM module.

    $ npm install follow

## Example

This looks much like the [request][req] API.

```javascript
var follow = require('follow');
follow("https://example.iriscouch.com/boogie", function(error, change) {
  if(!error) {
    console.log("Got change number " + change.seq + ": " + change.id);
  }
})
```

The *error* parameter to the callback will basically always be `null`.

## Objective

The API must be very simple: notify me every time a change happens in the DB. Also, never fail.

If an error occurs, Follow will internally retry without notifying your code.

Specifically, this should be possible:

1. Begin a changes feed. Get a couple of change callbacks
2. Shut down CouchDB
3. Go home. Have a nice weekend. Come back on Monday.
4. Start CouchDB with a different IP address
5. Make a couple of changes
6. Update DNS so the domain points to the new IP
7. Once DNS propagates, get a couple more change callbacks

## Failure Mode

If CouchDB permanently crashes, there is an option of failure modes:

* **Default:** Simply never call back with a change again
* **Optional:** Specify an *inactivity* timeout. If no changes happen by the timeout, Follow will signal an error.

## DB Updates

If the db url ends with `/_db_updates`, Follow will provide a
[_db_updates](http://docs.couchdb.org/en/latest/api/server/common.html?highlight=db_updates#get--_db_updates) feed.

For each change, Follow will emit a `change` event containing:

* `type`: `created`, `updated` or `deleted`.
* `db_name`: Name of the database where the change occoured.
* `ok`: Event operation status (boolean).

Note that this feature is available as of CouchDB 1.4.

### Simple API: follow(options, callback)

The first argument is an options object. The only required option is `db`. Instead of an object, you can use a string to indicate the `db` value.

```javascript
follow({db:"https://example.iriscouch.com/boogie", include_docs:true}, function(error, change) {
  if(!error) {
    console.log("Change " + change.seq + " has " + Object.keys(change.doc).length + " fields");
  }
})
```

<a name="options"></a>
All of the CouchDB _changes options are allowed. See http://guide.couchdb.org/draft/notifications.html.

* `db` | Fully-qualified URL of a couch database. (Basic auth URLs are ok.)
* `since` | The sequence number to start from. Use `"now"` to start from the latest change in the DB.
* `heartbeat` | Milliseconds within which CouchDB must respond (default: **30000** or 30 seconds)
* `feed` | **Optional but only "continuous" is allowed**
* `filter` |
  * **Either** a path to design document filter, e.g. `app/important`
  * **Or** a Javascript `function(doc, req) { ... }` which should return true or false
* `query_params` | **Optional** for use in with `filter` functions, passed as `req.query` to the filter function

Besides the CouchDB options, more are available:

* `headers` | Object with HTTP headers to add to the request
* `inactivity_ms` | Maximum time to wait between **changes**. Omitting this means no maximum.
* `max_retry_seconds` | Maximum time to wait between retries (default: 360 seconds)
* `initial_retry_delay` | Time to wait before the first retry, in milliseconds (default 1000 milliseconds)
* `response_grace_time` | Extra time to wait before timing out, in milliseconds (default 5000 milliseconds)

## Object API

The main API is a thin wrapper around the EventEmitter API.

```javascript
var follow = require('follow');

var opts = {}; // Same options paramters as before
var feed = new follow.Feed(opts);

// You can also set values directly.
feed.db            = "http://example.iriscouch.com/boogie";
feed.since         = 3;
feed.heartbeat     = 30    * 1000
feed.inactivity_ms = 86400 * 1000;

feed.filter = function(doc, req) {
  // req.query is the parameters from the _changes request and also feed.query_params.
  console.log('Filtering for query: ' + JSON.stringify(req.query));

  if(doc.stinky || doc.ugly)
    return false;
  return true;
}

feed.on('change', function(change) {
  console.log('Doc ' + change.id + ' in change ' + change.seq + ' is neither stinky nor ugly.');
})

feed.on('error', function(er) {
  console.error('Since Follow always retries on errors, this must be serious');
  throw er;
})

feed.follow();
```

<a name="pause"></a>
## Pause and Resume

A Follow feed is a Node.js stream. If you get lots of changes and processing them takes a while, use `.pause()` and `.resume()` as needed. Pausing guarantees that no new events will fire. Resuming guarantees you'll pick up where you left off.

```javascript
follow("https://example.iriscouch.com/boogie", function(error, change) {
  var feed = this

  if(change.seq == 1) {
    console.log('Uh oh. The first change takes 30 hours to process. Better pause.')
    feed.pause()
    setTimeout(function() { feed.resume() }, 30 * 60 * 60 * 1000)
  }

  // ... 30 hours with no events ...

  else
    console.log('No need to pause for normal change: ' + change.id)
})
```

<a name="events"></a>
## Events

The feed object is an EventEmitter. There are a few ways to get a feed object:

* Use the object API above
* Use the return value of `follow()`
* In the callback to `follow()`, the *this* variable is bound to the feed object.

Once you've got one, you can subscribe to these events:

* **start** | Before any i/o occurs
* **confirm_request** | `function(req)` | The database confirmation request is sent; passed the `request` object
* **confirm** | `function(db_obj)` | The database is confirmed; passed the couch database object
* **change** | `function(change)` | A change occured; passed the change object from CouchDB
* **catchup** | `function(seq_id)` | The feed has caught up to the *update_seq* from the confirm step. Assuming no subsequent changes, you have seen all the data.
* **wait** | Follow is idle, waiting for the next data chunk from CouchDB
* **timeout** | `function(info)` | Follow did not receive a heartbeat from couch in time. The passed object has `.elapsed_ms` set to the elapsed time
* **retry** | `function(info)` | A retry is scheduled (usually after a timeout or disconnection). The passed object has
  * `.since` the current sequence id
  * `.after` the milliseconds to wait before the request occurs (on an exponential fallback schedule)
  * `.db` the database url (scrubbed of basic auth credentials)
* **stop** | The feed is stopping, because of an error, or because you called `feed.stop()`
* **error** | `function(err)` | An error occurs

## Error conditions

Follow is happy to retry over and over, for all eternity. It will only emit an error if it thinks your whole application might be in trouble.

* *DB confirmation* failed: Follow confirms the DB with a preliminary query, which must reply properly.
* *DB is deleted*: Even if it retried, subsequent sequence numbers would be meaningless to your code.
* *Your inactivity timer* expired: This is a last-ditch way to detect possible errors. What if couch is sending heartbeats just fine, but nothing has changed for 24 hours? You know that for your app, 24 hours with no change is impossible. Maybe your filter has a bug? Maybe you queried the wrong DB? Whatever the reason, Follow will emit an error.
* JSON parse error, which should be impossible from CouchDB
* Invalid change object format, which should be impossible from CouchDB
* Internal error, if the internal state seems wrong, e.g. cancelling a timeout that already expired, etc. Follow tries to fail early.

## Tests

Follow uses [node-tap][tap]. If you clone this Git repository, tap is included.

    $ ./node_modules/.bin/tap test/*.js test/issues/*.js
    ok test/couch.js ...................................... 11/11
    ok test/follow.js ..................................... 69/69
    ok test/issues.js ..................................... 44/44
    ok test/stream.js ................................... 300/300
    ok test/issues/10.js .................................. 11/11
    total ............................................... 435/435

    ok

## License

Apache 2.0

[req]: https://github.com/mikeal/request
[tap]: https://github.com/isaacs/node-tap
