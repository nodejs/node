window.process = { env: {} };

if(!Object.keys)
  Object.keys = function(o){
    if(typeof o !== 'object')
      throw new Error('Object.keys called on non-object: ' + JSON.stringify(o));
    var ret=[], p;
    for (p in o)
      if(Object.prototype.hasOwnProperty.call(o,p))
        ret.push(p);
    return ret;
  }

if(!Array.isArray)
  Array.isArray = function(o) {
    return Object.prototype.toString.call(o) === '[object Array]';
  }

if(!Array.prototype.forEach)
  Array.prototype.forEach = function(callback) {
    var i, len = this.length;
    for(var i = 0; i < len; i++)
      callback(this[i], i, this);
  }

if(!Array.prototype.reduce)
  Array.prototype.reduce = function(callback, state) {
    var i, len = this.length;
    for(i = 0; i < len; i++)
      state = callback(state, this[i]);
    return state;
  }

if(!Array.prototype.filter)
  Array.prototype.filter = function(pred) {
    var i, len = this.length, result = [];
    for(i = 0; i < len; i++)
      if(!! pred(this[i]))
        result.push(this[i]);
    return result;
  }

if(!Array.prototype.map)
  Array.prototype.map = function(func) {
    var i, len = this.length, result = [];
    for(i = 0; i < len; i++)
      result.push(func(this[i], i, this));
    return result;
  }


if(!window.console)
  window.console = {};

; ['trace', 'debug', 'log', 'info', 'warn', 'error', 'fatal'].forEach(function(lev) {
  window.console[lev] = window.console[lev] || function() {};
})

define(['events', 'querystring', 'follow/cli'], function(events, querystring, cli) {
  var welcome = [ 'Starting Follow.'
                , '<a href="https://github.com/iriscouch/follow">Follow Follow</a>'
                , 'at GitHub.'
                ];
  jQuery('#boot').html('<p>' + welcome.join(' ') + '</p>');

  // Set up some faux Node stuff.
  events.EventEmitter = events.EventEmitter2;
  var process = window.process = new events.EventEmitter;

  process.env = querystring.parse(window.location.search.slice(1));

  process.stdout = {};
  process.stdout.write = function(x) {
    var con = jQuery('#con');
    con.append(x);
  }

  process.exit = function(code) {
    if(code === 0)
      console.log("'EXIT' " + code);
    else
      console.error("'EXIT' " + code);
  }


  return function() { // main()
    console.log('Main running');

    try       { cli.main() }
    catch(er) { console.log("Error starting tests"); console.log(er) }
  }
})
