// Core routines for event emitters
//
// Copyright 2011 Jason Smith, Jarrett Cruger and contributors
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.

var lib = require('../lib')
  , util = require('util')
  , events = require('events')
  , request = require('request')
  , Changes = require('./stream').Changes
  , querystring = require('querystring')

// Use the library timeout functions, primarily so the test suite can catch errors.
var setTimeout = lib.setTimeout
  , clearTimeout = lib.clearTimeout

var DEFAULT_HEARTBEAT = 30000;
var HEARTBEAT_TIMEOUT_COEFFICIENT = 1.25; // E.g. heartbeat 1000ms would trigger a timeout after 1250ms of no heartbeat.
var DEFAULT_MAX_RETRY_SECONDS     = 60 * 60;
var INITIAL_RETRY_DELAY           = 1000;
var RESPONSE_GRACE_TIME           = 5000;

var FEED_PARAMETERS   = ['since', 'limit', 'feed', 'heartbeat', 'filter', 'include_docs', 'view', 'style', 'conflicts', 'attachments', 'att_encoding_info'];

var EventEmitter = events.EventEmitter2 || events.EventEmitter;


util.inherits(Feed, EventEmitter);
function Feed (opts) {
  var self = this;
  EventEmitter.call(self);

  opts = opts || {}

  self.feed = 'continuous';
  self.heartbeat         = opts.heartbeat || DEFAULT_HEARTBEAT;
  self.max_retry_seconds = opts.max_retry_seconds || DEFAULT_MAX_RETRY_SECONDS;
  self.inactivity_ms = null;
  self.initial_retry_delay = opts.initial_retry_delay || INITIAL_RETRY_DELAY;
  self.response_grace_time = opts.response_grace_time || RESPONSE_GRACE_TIME;

  self.headers = {};
  self.request = opts.request || {} // Extra options for potentially future versions of request. The caller can supply them.

  self.since = 0;
  self.is_paused = false
  self.caught_up = false
  self.retry_delay = self.initial_retry_delay;

  self.query_params = {}; // Extra `req.query` values for filter functions

  if(typeof opts === 'string')
    opts = {'db': opts};

  Object.keys(opts).forEach(function(key) {
    if (typeof self[key] !== 'function')
      self[key] = opts[key];
  })

  self.pending = { request     : null
                 , activity_at : null
                 };
} // Feed

Feed.prototype.start =
Feed.prototype.follow = function follow_feed() {
  var self = this;

  self.db = self.db || self.url || self.uri
  delete self.url
  delete self.uri

  if(!self.db)
    throw new Error('Database URL required');

  if (self.db.match(/\/_db_updates$/))
    self.is_db_updates = true;

  if(self.is_db_updates)
    delete self.since;

  if(self.feed !== 'continuous' && self.feed !== 'longpoll')
    throw new Error('The only valid feed options are "continuous" and "longpoll"');

  if(typeof self.heartbeat !== 'number')
    throw new Error('Required "heartbeat" value');

  self.log = lib.log4js.getLogger(self.db);
  self.log.setLevel(process.env.follow_log_level || "info");

  self.emit('start');
  return self.confirm();
}

Feed.prototype.confirm = function confirm_feed() {
  var self = this;

  self.db_safe = lib.scrub_creds(self.db);

  var endpoint = self.is_db_updates ? 'server' : 'database';

  self.log.debug('Checking ' + endpoint + ': ' + self.db_safe);

  var confirm_timeout = self.heartbeat * 3; // Give it time to look up the name, connect, etc.
  var timeout_id = setTimeout(function() {
    return self.die(new Error('Timeout confirming ' + endpoint + ': ' + self.db_safe));
  }, confirm_timeout);

  var headers = lib.JP(lib.JS(self.headers));
  headers.accept = 'application/json';

  var uri = self.is_db_updates ? self.db.replace(/\/_db_updates$/, '') : self.db;
  var req = {'uri':uri, 'headers':headers}
  Object.keys(self.request).forEach(function(key) {
    req[key] = self.request[key];
  })

  req = request(req, db_response)
  self.emit('confirm_request', req)

  function db_response(er, resp, body) {
    clearTimeout(timeout_id);

    if(er)
      return self.die(er);

    var db;
    try {
      db = JSON.parse(body)
    } catch(json_er) {
      return self.emit('error', json_er)
    }

    if(!self.is_db_updates && !self.dead && (!db.db_name || !db.instance_start_time))
      return self.emit('error', new Error('Bad DB response: ' + body));

    if(self.is_db_updates && !self.dead && !db.couchdb)
      return self.emit('error', new Error('Bad server response: ' + body));

    if (!self.is_db_updates)
      self.original_db_seq = db.update_seq

    self.log.debug('Confirmed ' + endpoint + ': ' + self.db_safe);
    self.emit('confirm', db);

    if(self.since == 'now') {
      self.log.debug('Query since "now" is the same as query since -1')
      self.since = -1
    }

    if(self.since == -1) {
      self.log.debug('Query since '+self.since+' will start at ' + db.update_seq)
      self.since = db.update_seq
    } else if(self.since < 0) {
      if(isNaN(db.update_seq))
        return self.emit('error', new Error('DB requires specific id in "since"'));

      self.log.debug('Query since '+self.since+' will start at ' + (db.update_seq + self.since + 1))
      self.since = db.update_seq + self.since + 1
    }

    // If the next change would come after the current update_seq, just fake a catchup event now.
    if(self.original_db_seq == self.since) {
      self.caught_up = true
      self.emit('catchup', db.update_seq)
    }

    return self.query();
  }
}

Feed.prototype.query = function query_feed() {
  var self = this;

  var query_params = JSON.parse(JSON.stringify(self.query_params));

  FEED_PARAMETERS.forEach(function(key) {
    if(key in self)
      query_params[key] = self[key];
  })

  if(typeof query_params.filter !== 'string')
    delete query_params.filter;

  if(typeof self.filter === 'function' && !query_params.include_docs) {
    self.log.debug('Enabling include_docs for client-side filter');
    query_params.include_docs = true;
  }

  // Limit the response size for longpoll.
  var poll_size = 100;
  if(query_params.feed == 'longpoll' && (!query_params.limit || query_params.limit > poll_size))
    query_params.limit = poll_size;

  var feed_url = self.db + (self.is_db_updates ? '' : '/_changes') + '?' + querystring.stringify(query_params);

  self.headers.accept = self.headers.accept || 'application/json';
  var req = { method : 'GET'
            , uri    : feed_url
            , headers: self.headers
            , encoding: 'utf-8'
            }

  req.changes_query = query_params;
  Object.keys(self.request).forEach(function(key) {
    req[key] = self.request[key];
  })

  var now = new Date
    , feed_ts = lib.JDUP(now)
    , feed_id = process.env.follow_debug ? feed_ts.match(/\.(\d\d\d)Z$/)[1] : feed_ts

  self.log.debug('Feed query ' + feed_id + ': ' + lib.scrub_creds(feed_url))
  var feed_request = request(req)

  feed_request.on('response', function(res) {
    self.log.debug('Remove feed from agent pool: ' + feed_id)
    feed_request.req.socket.emit('agentRemove')

    // Simulate the old onResponse option.
    on_feed_response(null, res, res.body)
  })

  feed_request.on('error', on_feed_response)

  // The response headers must arrive within one heartbeat.
  var response_timer = setTimeout(response_timed_out, self.heartbeat + self.response_grace_time)
    , timed_out = false

  return self.emit('query', feed_request)

  function response_timed_out() {
    self.log.debug('Feed response timed out: ' + feed_id)
    timed_out = true
    return self.retry()
  }

  function on_feed_response(er, resp, body) {
    clearTimeout(response_timer)

    if((resp !== undefined && resp.body) || body)
      return self.die(new Error('Cannot handle a body in the feed response: ' + lib.JS(resp.body || body)))

    if(timed_out) {
      self.log.debug('Ignoring late response: ' + feed_id);
      return destroy_response(resp);
    }

    if(er) {
      self.log.debug('Request error ' + feed_id + ': ' + er.stack);
      destroy_response(resp);
      return self.retry();
    }

    if(resp.statusCode !== 200) {
      self.log.debug('Bad changes response ' + feed_id + ': ' + resp.statusCode);
      destroy_response(resp);
      return self.retry();
    }

    self.log.debug('Good response: ' + feed_id);
    self.retry_delay = self.initial_retry_delay;

    self.emit('response', resp);

    var changes_stream = new Changes
    changes_stream.log = lib.log4js.getLogger('stream ' + self.db)
    changes_stream.log.setLevel(self.log.level.levelStr)
    changes_stream.feed = self.feed
    feed_request.pipe(changes_stream)

    changes_stream.created_at = now
    changes_stream.id = function() { return feed_id }
    return self.prep(changes_stream)
  }
}

Feed.prototype.prep = function prep_request(changes_stream) {
  var self = this;

  var now = new Date;
  self.pending.request = changes_stream;
  self.pending.activity_at = now;
  self.pending.wait_timer  = null;

  // Just re-run the pause or resume to do the needful on changes_stream (self.pending.request).
  if(self.is_paused)
    self.pause()
  else
    self.resume()

  // The inactivity timer is for time between *changes*, or time between the
  // initial connection and the first change. Therefore it goes here.
  self.change_at = now;
  if(self.inactivity_ms) {
    clearTimeout(self.inactivity_timer);
    self.inactivity_timer = setTimeout(function() { self.on_inactivity() }, self.inactivity_ms);
  }

  changes_stream.on('heartbeat', handler_for('heartbeat'))
  changes_stream.on('error', handler_for('error'))
  changes_stream.on('data', handler_for('data'))
  changes_stream.on('end', handler_for('end'))

  return self.wait();

  function handler_for(ev) {
    var name = 'on_couch_' + ev;
    var inner_handler = self[name];

    return handle_confirmed_req_event;
    function handle_confirmed_req_event() {
      if(self.pending.request === changes_stream)
        return inner_handler.apply(self, arguments);

      if(!changes_stream.created_at)
        return self.die(new Error("Received data from unknown request")); // Pretty sure this is impossible.

      var s_to_now = (new Date() - changes_stream.created_at) / 1000;
      var s_to_req = '[no req]';
      if(self.pending.request)
        s_to_req = (self.pending.request.created_at - changes_stream.created_at) / 1000;

      var msg = ': ' + changes_stream.id() + ' to_req=' + s_to_req + 's, to_now=' + s_to_now + 's';

      if(ev == 'end' || ev == 'data' || ev == 'heartbeat') {
        self.log.debug('Old "' + ev + '": ' + changes_stream.id())
        return destroy_req(changes_stream)
      }

      self.log.warn('Old "'+ev+'"' + msg);
    }
  }
}

Feed.prototype.wait = function wait_for_event() {
  var self = this;
  self.emit('wait');

  if(self.pending.wait_timer)
    return self.die(new Error('wait() called but there is already a wait_timer: ' + self.pending.wait_timer));

  var timeout_ms = self.heartbeat * HEARTBEAT_TIMEOUT_COEFFICIENT;
  var req_id = self.pending.request && self.pending.request.id()
  var msg = 'Req ' + req_id + ' timeout=' + timeout_ms;
  if(self.inactivity_ms)
    msg += ', inactivity=' + self.inactivity_ms;
  msg += ': ' + self.db_safe;

  self.log.debug(msg);
  self.pending.wait_timer = setTimeout(function() { self.on_timeout() }, timeout_ms);
}

Feed.prototype.got_activity = function() {
  var self = this

  if (self.dead)
    return

  //
  // We may not have a wait_timer so just clear it and null it out if it does
  // exist
  //
  clearTimeout(self.pending.wait_timer)
  self.pending.wait_timer = null
  self.pending.activity_at = new Date
}


Feed.prototype.pause = function() {
  var self = this
    , was_paused = self.is_paused

  // Emit pause after pausing the stream, to allow listeners to react.
  self.is_paused = true
  if(self.pending && self.pending.request && self.pending.request.pause)
    self.pending.request.pause()
  else
    self.log.warn('No pending request to pause')

  if(!was_paused)
    self.emit('pause')
}

Feed.prototype.resume = function() {
  var self = this
    , was_paused = self.is_paused

  // Emit resume before resuming the data feed, to allow listeners to prepare.
  self.is_paused = false
  if(was_paused)
    self.emit('resume')

  if(self.pending && self.pending.request && self.pending.request.resume)
    self.pending.request.resume()
  else
    self.log.warn('No pending request to resume')
}


Feed.prototype.on_couch_heartbeat = function on_couch_heartbeat() {
  var self = this

  self.got_activity()
  if(self.dead)
    return self.log.debug('Skip heartbeat processing for dead feed')

  self.emit('heartbeat')

  if(self.dead)
    return self.log.debug('No wait: heartbeat listener stopped this feed')
  self.wait()
}

Feed.prototype.on_couch_data = function on_couch_data(change) {
  var self = this;
  self.log.debug('Data from ' + self.pending.request.id());

  self.got_activity()
  if(self.dead)
    return self.log.debug('Skip data processing for dead feed')

  // The changes stream guarantees that this data is valid JSON.
  change = JSON.parse(change)

  //self.log.debug('Object:\n' + util.inspect(change));
  if(!self.is_db_updates && 'last_seq' in change) {
    self.log.warn('Stopping upon receiving a final message: ' + JSON.stringify(change))
    var del_er = new Error('Database deleted after change: ' + change.last_seq)
    del_er.deleted = true
    del_er.last_seq = change.last_seq
    return self.die(del_er)
  }

  if(!self.is_db_updates && !change.seq)
    return self.die(new Error('Change has no .seq field: ' + JSON.stringify(change)))

  self.on_change(change)

  // on_change() might work its way all the way to a "change" event, and the listener
  // might call .stop(), which means among other things that no more events are desired.
  // The die() code sets a self.dead flag to indicate this.
  if(self.dead)
    return self.log.debug('No wait: change listener stopped this feed')
  self.wait()
}

Feed.prototype.on_timeout = function on_timeout() {
  var self = this;
  if (self.dead)
    return self.log.debug('No timeout: change listener stopped this feed');

  self.log.debug('Timeout')

  var now = new Date;
  var elapsed_ms = now - self.pending.activity_at;

  self.emit('timeout', {elapsed_ms:elapsed_ms, heartbeat:self.heartbeat, id:self.pending.request.id()});

  /*
  var msg = ' for timeout after ' + elapsed_ms + 'ms; heartbeat=' + self.heartbeat;
  if(!self.pending.request.id)
    self.log.warn('Closing req (no id) ' + msg + ' req=' + util.inspect(self.pending.request));
  else
    self.log.warn('Closing req ' + self.pending.request.id() + msg);
  */

  destroy_req(self.pending.request);
  self.retry()
}

Feed.prototype.retry = function retry() {
  var self = this;

  clearTimeout(self.pending.wait_timer);
  self.pending.wait_timer = null;

  self.log.debug('Retry since=' + self.since + ' after ' + self.retry_delay + 'ms ')
  self.emit('retry', {since:self.since, after:self.retry_delay, db:self.db_safe});

  self.retry_timer = setTimeout(function() { self.query() }, self.retry_delay);

  var max_retry_ms = self.max_retry_seconds * 1000;
  self.retry_delay *= 2;
  if(self.retry_delay > max_retry_ms)
    self.retry_delay = max_retry_ms;
}

Feed.prototype.on_couch_end = function on_couch_end() {
  var self = this;

  self.log.debug('Changes feed ended ' + self.pending.request.id());
  self.pending.request = null;
  return self.retry();
}

Feed.prototype.on_couch_error = function on_couch_error(er) {
  var self = this;

  self.log.debug('Changes query eror: ' + lib.JS(er.stack));
  return self.retry();
}

Feed.prototype.stop = function(val) {
  var self = this
  self.log.debug('Stop')

  // Die with no errors.
  self.die()
  self.emit('stop', val);
}

Feed.prototype.die = function(er) {
  var self = this;

  if(er)
    self.log.fatal('Fatal error: ' + er.stack);

  // Warn code executing later that death has occured.
  self.dead = true

  clearTimeout(self.retry_timer)
  clearTimeout(self.inactivity_timer)
  clearTimeout(self.pending.wait_timer)

  self.inactivity_timer = null
  self.pending.wait_timer = null

  var req = self.pending.request;
  self.pending.request = null;
  if(req) {
    self.log.debug('Destroying req ' + req.id());
    destroy_req(req);
  }

  if(er)
    self.emit('error', er);
}

Feed.prototype.on_change = function on_change(change) {
  var self = this;

  if(!self.is_db_updates && !change.seq)
    return self.die(new Error('No seq value in change: ' + lib.JS(change)));

  if(!self.is_db_updates && change.seq == self.since) {
    self.log.debug('Bad seq value ' + change.seq + ' since=' + self.since);
    return destroy_req(self.pending.request);
  }

  if(typeof self.filter !== 'function')
    return self.on_good_change(change);

  var req = lib.JDUP({'query': self.pending.request.changes_query});
  var filter_args;

  if (self.is_db_updates) {
    if(!change.db_name || !change.type)
      return self.die(new Error('Internal _db_updates filter needs .db_name and .type in change ', change));
    filter_args = [change.db_name, change.type, req];
  } else {
    if(!change.doc)
      return self.die(new Error('Internal filter needs .doc in change ' + change.seq));

    // Don't let the filter mutate the real data.
    var doc = lib.JDUP(change.doc);
    filter_args = [doc, req];
  }
  var result = false;
  try {
    result = self.filter.apply(null, filter_args);
  } catch (er) {
    self.log.debug('Filter error', er);
  }

  result = (result && true) || false;
  if(result) {
    self.log.debug('Builtin filter PASS for change: ' + change.seq);
    return self.on_good_change(change);
  } else {
    self.log.debug('Builtin filter FAIL for change: ' + change.seq);

    // Even with a filtered change, a "catchup" event might still be appropriate.
    self.check_for_catchup(change.seq)
  }
}

Feed.prototype.on_good_change = function on_good_change(change) {
  var self = this;

  if(self.inactivity_ms && !self.inactivity_timer)
    return self.die(new Error('Cannot find inactivity timer during change'));

  clearTimeout(self.inactivity_timer);
  self.inactivity_timer = null;
  if(self.inactivity_ms)
    self.inactivity_timer = setTimeout(function() { self.on_inactivity() }, self.inactivity_ms);

  self.change_at = new Date;

  if(!self.is_db_updates)
    self.since = change.seq;

  self.emit('change', change);

  self.check_for_catchup(change.seq)
}

Feed.prototype.check_for_catchup = function check_for_catchup(seq) {
  var self = this

  if (self.is_db_updates)
    return
  if(self.caught_up)
    return
  if(seq < self.original_db_seq)
    return

  self.caught_up = true
  self.emit('catchup', seq)
}

Feed.prototype.on_inactivity = function on_inactivity() {
  var self = this;
  var now = new Date;
  var elapsed_ms = now - self.change_at;
  var elapsed_s  = elapsed_ms / 1000;

  //
  // Since this is actually not fatal, lets just totally reset and start a new
  // request, JUST in case something was bad.
  //
  self.log.debug('Req ' + self.pending.request.id() + ' made no changes for ' + elapsed_s + 's');
  return self.restart();

}

Feed.prototype.restart = function restart() {
  var self = this

  self.emit('restart')

  // Kill ourselves and then start up once again
  self.stop()
  self.dead = false
  self.start()
}

module.exports = { "Feed" : Feed
                 };


/*
 * Utilities
 */

function destroy_req(req) {
  if(req)
    destroy_response(req.response)

  if(req && typeof req.destroy == 'function')
    req.destroy()
}

function destroy_response(response) {
  if(!response)
    return;

  if(typeof response.abort === 'function')
    response.abort();

  if(response.connection)
    response.connection.destroy();
}
