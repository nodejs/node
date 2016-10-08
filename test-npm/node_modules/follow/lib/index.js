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

exports.scrub_creds = function scrub_creds(url) {
  return url.replace(/^(https?:\/\/)[^:]+:[^@]+@(.*)$/, '$1$2'); // Scrub username and password
}

exports.JP = JSON.parse;
exports.JS = JSON.stringify;
exports.JDUP = function(obj) { return JSON.parse(JSON.stringify(obj)) };

var timeouts = { 'setTimeout': setTimeout
               , 'clearTimeout': clearTimeout
               }

exports.setTimeout   = function() { return timeouts.setTimeout.apply(this, arguments) }
exports.clearTimeout = function() { return timeouts.clearTimeout.apply(this, arguments) }
exports.timeouts = function(set, clear) {
  timeouts.setTimeout = set
  timeouts.clearTimeout = clear
}

var debug = require('debug')

function getLogger(name) {
  return { "trace": noop
         , "debug": debug('follow:' + name + ':debug')
         , "info" : debug('follow:' + name + ':info')
         , "warn" : debug('follow:' + name + ':warn')
         , "error": debug('follow:' + name + ':error')
         , "fatal": debug('follow:' + name + ':fatal')

         , "level": {'level':0, 'levelStr':'noop'}
         , "setLevel": noop
         }
}

function noop () {}

exports.log4js = { 'getLogger': getLogger }
