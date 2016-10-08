// Changes stream
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
  , stream = require('stream')
  , request = require('request')

// Use the library timeout functions, primarily so the test suite can catch errors.
var setTimeout = lib.setTimeout
  , clearTimeout = lib.clearTimeout


var DEFS =
  { 'longpoll_header': '{"results":['
  , 'log_level'      : process.env.follow_log_level || 'info'
  }

module.exports = { 'Changes': Changes
                 }


util.inherits(Changes, stream)
function Changes (opts) {
  var self = this
  stream.call(self)

  self.readable = true
  self.writable = true

  self.headers = {}
  self.statusCode = null

  opts = opts || {}
  self.feed = opts.feed || null // "continuous" or "longpoll"
  self.encoding = opts.encoding || 'utf8'

  self.log = opts.log
  if(!self.log) {
    self.log = lib.log4js.getLogger('change_stream')
    self.log.setLevel(DEFS.log_level)
  }

  self.is_sending = true
  self.is_ending = false
  self.is_dead = false

  self.source = null
  self.expect = null
  self.buf = null
  self.changes = []

  self.on('pipe', function(src) {
    if(!self.source)
      self.source = src
    else {
      var er = new Error('Already have a pipe source')
      er.source = self.source
      self.error(er)
    }
  })
}


Changes.prototype.setHeader = function(key, val) {
  var self = this
  self.headers[key] = val
}

//
// Readable stream API
//

Changes.prototype.setEncoding = function(encoding) {
  var self = this
  self.encoding = encoding // TODO
}


Changes.prototype.pause = function() {
  var self = this
  self.is_sending = false

  if(self.source && self.source.pause)
    self.source.pause()
}


Changes.prototype.resume = function() {
  var self = this
  self.is_sending = true
  if(self.source && self.source.resume)
    self.source.resume()
  self.emit_changes()
}

//
// Writable stream API
//

Changes.prototype.write = function(data, encoding) {
  var self = this

  data = self.normalize_data(data, encoding)
  if(typeof data != 'string')
    return // Looks like normalize_data emitted an error.

  if(self.feed === 'longpoll')
    return self.write_longpoll(data)
  else if(self.feed === 'continuous')
    return self.write_continuous(data)
}


Changes.prototype.write_longpoll = function(data) {
  var self = this

  if(self.buf === null)
    self.buf = []

  self.buf.push(data)
  return true
}


Changes.prototype.write_continuous = function(data) {
  var self = this

  var offset, json, change
    , buf = (self.buf || "") + data

  self.log.debug('write: ' + util.inspect({'data':data, 'buf':buf}))

  // Buf could have 0, 1, or many JSON objects in it.
  while((offset = buf.indexOf("\n")) >= 0) {
    json = buf.substr(0, offset);
    buf = buf.substr(offset + 1);
    self.log.debug('JSON: ' + util.inspect(json))

    // Heartbeats (empty strings) are fine, but otherwise confirm valid JSON.
    if(json === "")
      ;

    else if(json[0] !== '{')
      return self.error(new Error('Non-object JSON data: ' + json))

    else {
      try { change = JSON.parse(json) }
      catch (er) { return self.error(er) }

      self.log.debug('Object: ' + util.inspect(change))
      json = JSON.stringify(change)
    }

    // Change (or heartbeat) looks good.
    self.changes.push(json)
  }

  // Remember the unused data and send all known good changes (or heartbeats). The data (or heartbeat)
  // event listeners may call .pause() so remember the is_sending state now before calling them.
  var was_sending = self.is_sending
  self.buf = buf
  self.emit_changes()
  return was_sending
}


Changes.prototype.end = function(data, encoding) {
  var self = this

  self.is_ending = true
  self.writable = false

  // Always call write, even with no data, so it can fire the "end" event.
  self.write(data, encoding)

  if(self.feed === 'longpoll') {
    var changes = [ DEFS.longpoll_header ].concat(self.buf).join('')
    try { changes = JSON.parse(changes) || {} }
    catch (er) { return self.error(er) }

    if(!Array.isArray(changes.results))
      return self.error(new Error('No "results" field in feed'))
    if(self.changes.length !== 0)
      return self.error(new Error('Changes are already queued: ' + JSON.stringify(self.changes)))

    self.changes = changes.results.map(function(change) { return JSON.stringify(change) })
    return self.emit_changes()
  }

  else if(self.feed === 'continuous') {
    if(self.buf !== "")
      self.log.debug('Unprocessed data after "end" called: ' + util.inspect(self.buf))
  }
}


Changes.prototype.emit_changes = function() {
  var self = this

  while(self.is_sending && self.changes.length > 0) {
    var change = self.changes.shift()
    if(change === "") {
      self.log.debug('emit: heartbeat')
      self.emit('heartbeat')
    }

    else {
      self.log.debug('emit: data')
      self.emit('data', change)
    }
  }

  if(self.is_sending && self.is_ending && self.changes.length === 0) {
    self.is_ending = false
    self.readable = false
    self.log.debug('emit: end')
    self.emit('end')
  }
}

//
// Readable/writable stream API
//

Changes.prototype.destroy = function() {
  var self = this
  self.log.debug('destroy')

  self.is_dead = true
  self.is_ending = false
  self.is_sending = false

  if(self.source && typeof self.source.abort == 'function')
    return self.source.abort()

  if(self.source && typeof self.source.destroy === 'function')
    self.source.destroy()

  // Often the source is from the request package, so destroy its response object.
  if(self.source && self.source.__isRequestRequest && self.source.response
  && typeof self.source.response.destroy === 'function')
    self.source.response.destroy()
}


Changes.prototype.destroySoon = function() {
  var self = this
  throw new Error('not implemented')
  //return self.request.destroySoon()
}

//
// Internal implementation
//

Changes.prototype.normalize_data = function(data, encoding) {
  var self = this

  if(data instanceof Buffer)
    data = data.toString(encoding)
  else if(typeof data === 'undefined' && typeof encoding === 'undefined')
    data = ""

  if(typeof data != 'string')
    return self.error(new Error('Not a string or Buffer: ' + util.inspect(data)))

  if(self.feed !== 'continuous' && self.feed !== 'longpoll')
    return self.error(new Error('Must set .feed to "continuous" or "longpoll" before writing data'))

  if(self.expect === null)
    self.expect = (self.feed == 'longpoll')
                    ? DEFS.longpoll_header
                    : ""

  var prefix = data.substr(0, self.expect.length)
  data = data.substr(prefix.length)

  var expected_part = self.expect.substr(0, prefix.length)
    , expected_remainder = self.expect.substr(expected_part.length)

  if(prefix !== expected_part)
    return self.error(new Error('Prefix not expected '+util.inspect(expected_part)+': ' + util.inspect(prefix)))

  self.expect = expected_remainder
  return data
}


Changes.prototype.error = function(er) {
  var self = this

  self.readable = false
  self.writable = false
  self.emit('error', er)

  // The write() method sometimes returns this value, so if there was an error, make write() return false.
  return false
}
