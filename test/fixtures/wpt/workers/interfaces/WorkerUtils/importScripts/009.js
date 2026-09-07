var log = postMessage;
importScripts('data:text/javascript,function run() { for(var i = 0; i < 1000; ++i) { if (i == 500) log(true); } return 1; }');
postMessage(run());