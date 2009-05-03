node.http = {
  fillField : function (msg, field, data) {
    msg[field] = (msg[field] || "") + data;
  },

  appendHeaderField : function (msg, data) {
    if (msg.hasOwnProperty("headers")) {
      var last_pair = msg.headers[msg.headers.length-1];
      if (last_pair.length == 1)
        last_pair[0] += data;
      else 
        msg.headers.push([data]);
    } else {
      msg.headers = [[data]];
    }
  },

  appendHeaderValue : function (msg, data) {
    var last_pair = msg.headers[msg.headers.length-1];
    if (last_pair.length == 1)
      last_pair[1] = data;
    else 
      last_pair[1] += data;
  }
};
