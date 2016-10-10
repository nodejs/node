// log4js stub
//

define(['querystring'], function(querystring) {
  function noop() {};
  function trace(str) { return console.trace(str) }
  function log(str) { return console.log(str) }

  function is_verbose() {
    return !! querystring.parse(window.location.search).verbose;
  }

  function con(str) {
    var con = jQuery('#con');
    var html = con.html();
    con.html(html + str + "<br>");
  }

  function debug(str) {
    if(is_verbose())
      con(str);
    return console.debug(str);
  }

  function out(str) {
    con(str);
    log(str);
  }

  var err = out;

  var noops = { "trace": trace
              , "debug": debug
              , "info" : out
              , "warn" : err
              , "error": err
              , "fatal": err

              , "setLevel": noop
              }

  return function() {
    return { 'getLogger': function() { return noops }
           }
  }
})
