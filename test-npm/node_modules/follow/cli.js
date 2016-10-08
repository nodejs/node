#!/usr/bin/env node
// The follow command-line interface.
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

var lib = require('./lib')
  , couch_changes = require('./api')
  ;

function puts(str) {
  process.stdout.write(str + "\n");
}

function main() {
  var db = require.isBrowser ? (process.env.db || '/_users') : process.argv[2];
  puts('Watching: ' + db);

  var feed = new couch_changes.Feed();
  feed.db = db;
  feed.since = (process.env.since === 'now') ? 'now' : parseInt(process.env.since || '0');

  feed.heartbeat = (process.env.heartbeat || '3000').replace(/s$/, '000');
  feed.heartbeat = parseInt(feed.heartbeat);

  if(require.isBrowser)
    feed.feed = 'longpoll';
  if(process.env.host)
    feed.headers.host = process.env.host;
  if(process.env.inactivity)
    feed.inactivity_ms = parseInt(process.env.inactivity);
  if(process.env.limit)
    feed.limit = parseInt(process.env.limit);

  feed.query_params.pid = process.pid;
  feed.filter = process.env.filter || example_filter;
  function example_filter(doc, req) {
    // This is a local filter. It runs on the client side.
    var label = 'Filter ' + (req.query.pid || '::');

    if(process.env.show_doc)
      console.log(label + ' doc: ' + JSON.stringify(doc));
    if(process.env.show_req)
      console.log(label + ' for ' + doc._id + ' req: ' + JSON.stringify(req));
    return true;
  }

  feed.on('confirm', function() {
    puts('Database confirmed: ' + db);
  })

  feed.on('change', function(change) {
    puts('Change:' + JSON.stringify(change));
  })

  feed.on('timeout', function(state) {
    var seconds = state.elapsed_ms / 1000;
    var hb = state.heartbeat / 1000;
    puts('Timeout after ' + seconds + 's inactive, heartbeat=' + hb + 's');
  })

  feed.on('retry', function(state) {
    if(require.isBrowser)
      puts('Long polling since ' + state.since);
    else
      puts('Retry since ' + state.since + ' after ' + state.after + 'ms');
  })

  feed.on('response', function() {
    puts('Streaming response:');
  })

  feed.on('error', function(er) {
    //console.error(er);
    console.error('Changes error ============\n' + er.stack);
    setTimeout(function() { process.exit(0) }, 100);
  })

  process.on('uncaughtException', function(er) {
    puts('========= UNCAUGHT EXCEPTION; This is bad');
    puts(er.stack);
    setTimeout(function() { process.exit(1) }, 100);
  })

  feed.follow();
}

exports.main = main;
if(!require.isBrowser && process.argv[1] == module.filename)
  main();
