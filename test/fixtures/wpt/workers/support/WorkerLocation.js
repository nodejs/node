var obj = new Object();
obj.location = location.toString();
obj.href     = location.href;
obj.origin   = location.origin;
obj.protocol = location.protocol;
obj.host     = location.host;
obj.hostname = location.hostname;
obj.port     = location.port;
obj.pathname = location.pathname;
obj.search   = location.search;
obj.hash     = location.hash;

postMessage(obj);
