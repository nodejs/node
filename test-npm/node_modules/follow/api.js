// The changes_couchdb API
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

var feed = require('./lib/feed')
  , stream = require('./lib/stream')

function follow_feed(opts, cb) {
  var ch_feed = new feed.Feed(opts);
  ch_feed.on('error' , function(er) { return cb && cb.call(ch_feed, er) });
  ch_feed.on('change', function(ch) { return cb && cb.call(ch_feed, null, ch) });

  // Give the caller a chance to hook into any events.
  process.nextTick(function() {
    ch_feed.follow();
  })

  return ch_feed;
}

module.exports = follow_feed;
module.exports.Feed = feed.Feed;
module.exports.Changes = stream.Changes
