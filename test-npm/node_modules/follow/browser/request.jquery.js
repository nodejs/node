(function() {
var define = window.define
if(!define) define = function(deps, definer) {
  if(!window.jQuery)
    throw new Error("Can't find jQuery");
  return definer(window.jQuery);
}

define(['jquery'], function(jQuery) {

//
// request.jquery
//

var DEFAULT_TIMEOUT = 3 * 60 * 1000; // 3 minutes

function request(options, callback) {
  var options_onResponse = options.onResponse; // Save this for later.

  if(typeof options === 'string')
    options = {'uri':options};
  else
    options = JSON.parse(JSON.stringify(options)); // Use a duplicate for mutating.

  if(options.url) {
    options.uri = options.url;
    delete options.url;
  }

  if(!options.uri && options.uri !== "")
    throw new Error("options.uri is a required argument");

  if(options.json) {
    options.body = JSON.stringify(options.json);
    delete options.json;
  }

  if(typeof options.uri != "string")
    throw new Error("options.uri must be a string");

  ; ['proxy', '_redirectsFollowed', 'maxRedirects', 'followRedirect'].forEach(function(opt) {
    if(options[opt])
      throw new Error("options." + opt + " is not supported");
  })

  options.method = options.method || 'GET';
  options.headers = options.headers || {};

  if(options.headers.host)
    throw new Error("Options.headers.host is not supported");

  // onResponse is just like the callback but that is not quite what Node request does.
  callback = callback || options_onResponse;

  /*
  // Browsers do not like this.
  if(options.body)
    options.headers['content-length'] = options.body.length;
  */

  var headers = {};
  var beforeSend = function(xhr, settings) {
    if(!options.headers.authorization && options.auth) {
      debugger
      options.headers.authorization = 'Basic ' + b64_enc(options.auth.username + ':' + options.auth.password);
    }

    for (var key in options.headers)
      xhr.setRequestHeader(key, options.headers[key]);
  }

  // Establish a place where the callback arguments will go.
  var result = [];

  function fix_xhr(xhr) {
    var fixed_xhr = {};
    for (var key in xhr)
      fixed_xhr[key] = xhr[key];
    fixed_xhr.statusCode = xhr.status;
    return fixed_xhr;
  }

  var onSuccess = function(data, reason, xhr) {
    result = [null, fix_xhr(xhr), data];
  }

  var onError = function (xhr, reason, er) {
    var body = undefined;

    if(reason == 'timeout') {
      er = er || new Error("Request timeout");
    } else if(reason == 'error') {
      if(xhr.status > 299 && xhr.responseText.length > 0) {
        // Looks like HTTP worked, so there is no error as far as request is concerned. Simulate a success scenario.
        er = null;
        body = xhr.responseText;
      }
    } else {
      er = er || new Error("Unknown error; reason = " + reason);
    }

    result = [er, fix_xhr(xhr), body];
  }

  var onComplete = function(xhr, reason) {
    if(result.length === 0)
      result = [new Error("Result does not exist at completion time")];
    return callback && callback.apply(this, result);
  }


  var cors_creds = !!( options.creds || options.withCredentials );

  return jQuery.ajax({ 'async'      : true
                     , 'cache'      : (options.cache || false)
                     , 'contentType': (options.headers['content-type'] || 'application/x-www-form-urlencoded')
                     , 'type'       : options.method
                     , 'url'        : options.uri
                     , 'data'       : (options.body || undefined)
                     , 'timeout'    : (options.timeout || request.DEFAULT_TIMEOUT)
                     , 'dataType'   : 'text'
                     , 'processData': false
                     , 'beforeSend' : beforeSend
                     , 'success'    : onSuccess
                     , 'error'      : onError
                     , 'complete'   : onComplete
                     , 'xhrFields'  : { 'withCredentials': cors_creds
                                      }
                     });

};

request.withCredentials = false;
request.DEFAULT_TIMEOUT = DEFAULT_TIMEOUT;

var shortcuts = [ 'get', 'put', 'post', 'head' ];
shortcuts.forEach(function(shortcut) {
  var method = shortcut.toUpperCase();
  var func   = shortcut.toLowerCase();

  request[func] = function(opts) {
    if(typeof opts === 'string')
      opts = {'method':method, 'uri':opts};
    else {
      opts = JSON.parse(JSON.stringify(opts));
      opts.method = method;
    }

    var args = [opts].concat(Array.prototype.slice.apply(arguments, [1]));
    return request.apply(this, args);
  }
})

request.json = function(options, callback) {
  options = JSON.parse(JSON.stringify(options));
  options.headers = options.headers || {};
  options.headers['accept'] = options.headers['accept'] || 'application/json';

  if(options.method !== 'GET')
    options.headers['content-type'] = 'application/json';

  return request(options, function(er, resp, body) {
    if(!er)
      body = JSON.parse(body)
    return callback && callback(er, resp, body);
  })
}

request.couch = function(options, callback) {
  return request.json(options, function(er, resp, body) {
    if(er)
      return callback && callback(er, resp, body);

    if((resp.status < 200 || resp.status > 299) && body.error)
      // The body is a Couch JSON object indicating the error.
      return callback && callback(body, resp);

    return callback && callback(er, resp, body);
  })
}

jQuery(document).ready(function() {
  jQuery.request = request;
})

return request;

});

//
// Utility
//

// MIT License from http://phpjs.org/functions/base64_encode:358
function b64_enc (data) {
    // Encodes string using MIME base64 algorithm
    var b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";
    var o1, o2, o3, h1, h2, h3, h4, bits, i = 0, ac = 0, enc="", tmp_arr = [];

    if (!data) {
        return data;
    }

    // assume utf8 data
    // data = this.utf8_encode(data+'');

    do { // pack three octets into four hexets
        o1 = data.charCodeAt(i++);
        o2 = data.charCodeAt(i++);
        o3 = data.charCodeAt(i++);

        bits = o1<<16 | o2<<8 | o3;

        h1 = bits>>18 & 0x3f;
        h2 = bits>>12 & 0x3f;
        h3 = bits>>6 & 0x3f;
        h4 = bits & 0x3f;

        // use hexets to index into b64, and append result to encoded string
        tmp_arr[ac++] = b64.charAt(h1) + b64.charAt(h2) + b64.charAt(h3) + b64.charAt(h4);
    } while (i < data.length);

    enc = tmp_arr.join('');

    switch (data.length % 3) {
        case 1:
            enc = enc.slice(0, -2) + '==';
        break;
        case 2:
            enc = enc.slice(0, -1) + '=';
        break;
    }

    return enc;
}

})();
