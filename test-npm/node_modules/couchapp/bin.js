#!/usr/bin/env node

var couchapp = require('./main.js')
  , watch = require('watch')
  , path = require('path')
  , fs = require('fs')
  ;

function abspath (pathname) {
  if (pathname[0] === '/') return pathname
  return path.join(process.cwd(), path.normalize(pathname));
}

function copytree (source, dest) {
  watch.walk(source, function (err  , files) {
    for (i in files) {
      (function (i) {
        if (files[i].isDirectory()) {
          try {
            fs.mkdirSync(i.replace(source, dest), 0755)
          } catch(e) {
            console.log('Could not create '+dest)
          }
        } else {
          fs.readFile(i, function (err, data) {
            if (err) throw err;
            fs.writeFile(i.replace(source, dest), data, function (err) {
              if (err) throw err;
            });
          })
        }
      })(i);
    }
  })
}

function boiler (app) {
  if (app) {
    try { fs.mkdirSync(path.join(process.cwd(), app)) }
    catch(e) {};
  }
  app = app || '.'

  copytree(path.join(__dirname, 'boiler'), path.join(process.cwd(), app));


}


function onBeforePushSync() {
  if (beforePushSyncListener && typeof beforePushSyncListener.onBeforePushSync === "function") {
    beforePushSyncListener.onBeforePushSync();
  }
}
function onAfterPushSync() {
  if (afterPushSyncListener && typeof afterPushSyncListener.onAfterPushSync === "function") {
    afterPushSyncListener.onAfterPushSync();
  }
}
var _isUsingDirectoryConfig;
function isUsingDirectoryConfig() {
  if(_isUsingDirectoryConfig != null)
    return _isUsingDirectoryConfig;
  return _isUsingDirectoryConfig = (process.argv[2] && (process.argv[2].trim() === "-dc"));
}

if (process.mainModule && process.mainModule.filename === __filename) {
  var node
    , bin
    , command
    , app
    , couch
    , configDirectory
    , configFileNames
    , apps = []
    , beforePushSyncListener
    , afterPushSyncListener
    ;

  //check for directory-based config, if so then read rather than shift() the arguments: will need to read them again later
  if (isUsingDirectoryConfig()) {
    node = process.argv[0];
    bin = process.argv[1];
    command = process.argv[3];
    configDirectory = process.argv[4];
    couch = process.argv[5];
    configFileNames = fs.readdirSync(configDirectory);
    if (configFileNames) {
      configFileNames.forEach(function (value, index) {
        //any files starting with "app" are included as app files e.g. app.js, app_number1.js etc.
        if (value.indexOf("app") == 0) {
          apps.push(path.join(configDirectory, value));
        }
        //"before" listener must be called beforepushsync.js and be in the config directory
        else if (value.toLowerCase().trim() === "beforepushsync.js") {
          beforePushSyncListener = require(abspath(path.join(configDirectory, "beforepushsync.js")));
        }
        //"after" listener must be called afterpushsync.js and be in the config directory
        else if (value.toLowerCase().trim() === "afterpushsync.js") {
          afterPushSyncListener = require(abspath(path.join(configDirectory, "afterpushsync.js")));
        }
      });
    }
  }
  else {
    node = process.argv.shift();
    bin = process.argv.shift();
    command = process.argv.shift();
    app = process.argv.shift();
    couch = process.argv.shift();
  }

  if (command == 'help' || command == undefined) {
    console.log(
      [ "couchapp -- utility for creating couchapps"
        , ""
        , "Usage:"
        , "(backwardly compatible without switch - single app file)"
        , "  couchapp <command> app.js http://localhost:5984/dbname [opts]"
        , "(directory based config specified by switch - multiple app files and pre- and post-processing capability)"
        , " couchapp -dc <command> <appconfigdirectory> http://localhost:5984/dbname"
        , ""
        , "Commands:"
        , "  push   : Push app once to server."
        , "  sync   : Push app then watch local files for changes."
        , "  boiler : Create a boiler project."
        , "  serve  : Serve couchapp from development webserver"
        , "            you can specify some options "
        , "            -p port  : list on port portNum [default=3000]"
        , "            -d dir   : attachments directory [default='attachments']"
        , "            -l       : log rewrites to couchdb [default='false']"
      ]
      .join('\n')
    )
    process.exit();
  }
  
  if (couch == undefined) {
    try {
      couch = JSON.parse(fs.readFileSync('.couchapp.json')).couch;
    } catch (e) {
      // Discard exception: absent or malformed config file
    }
  }

  if (isUsingDirectoryConfig()) {
    if (command == 'boiler') {
      for (i in apps) {
        boiler(apps[i]);
      }
    } else {
      onBeforePushSync();
      for (i in apps) {
        //an immediately executed function is used so the loop counter variable is available
        //in createApp's callback function: multiple calls to push/sync are supported and
        //onAfterPushSync is supplied as the callback function on the last call
        (function keepLoopCounter(i) {
          couchapp.createApp(require(abspath(apps[i])), couch, function (app) {
            if (command == 'push') {
              app.push(i == apps.length - 1 ? onAfterPushSync : null);
            }
            else if (command == 'sync') {
              app.sync(i == apps.length ? onAfterPushSync : null);
            }
          });
        })(i);
      }

    }
  }
  else {
    if (command == 'boiler') {
      boiler(app);
    } else {
      couchapp.createApp(require(abspath(app)), couch, function (app) {
        if (command == 'push') app.push()
        else if (command == 'sync') app.sync()
        else if (command == 'serve') serve(app);
        
      })
    }
  }

}

// Start a development web server on app
function serve(app) {
  var url = require('url');
  var port = 3000,
      staticDir = 'attachments',
      tmpDir = '/tmp/couchapp-compile-' + process.pid,
      logDbRewrites = false;
  var arg;
  while(arg = process.argv.shift()){
    if(arg == '-p'){
      port = parseInt(process.argv.shift());
    }
    if(arg == '-d'){
      staticDir = process.argv.shift();
    }
    if(arg == '-l'){
      logDbRewrites = true;
    }
  }
  var dbUrlObj = url.parse(couch);

  var proxyPaths = {}
  var dbPrefix = '../../';
  for (var i in app.doc.rewrites){
    var rw = app.doc.rewrites[i];
    // Rewrites starting with '../../' are proxied to the database
    if (rw.to.indexOf(dbPrefix) == 0){
      var proxyPath = rw.from;
      if(proxyPath[proxyPath.length -1] == '*') {
        proxyPath = proxyPath.substring(0,proxyPath.length-1)
      }
      var dbPath = rw.to.substring(dbPrefix.length);
      if(dbPath[dbPath.length - 1] == '*'){
        dbPath = dbPath.substring(0,dbPath.length-1)
      }
      if(dbPath.indexOf('*') >=0 ){
        if(logDbRewrites){
          console.log("Don't know how to proxy '" + rw.from + "' to CouchDB at " + rw.from );
        }
      } else {
        proxyPaths[proxyPath] =
          dbUrlObj.protocol + '//' + dbUrlObj.host +
          path.normalize(dbUrlObj.pathname + dbPath);
          if(logDbRewrites){
            console.log("Proxying rewrite '" + proxyPath + "' to CouchDB at " + proxyPaths[proxyPath] );
          }
      }
    }
  }

  var connect = require('connect');
  var httpProxy = require('http-proxy'),
      connect   = require('connect'),
      connectCompiler;
  try{
      connectCompiler = require('connect-compiler');
  } catch(e) {}


  var proxy = new httpProxy.HttpProxy({
    target: {
      host: dbUrlObj.host,
      hostname: dbUrlObj.hostname,
      port: dbUrlObj.port,
      https: dbUrlObj.protocol == 'https:',
    }
  });
  var app = connect()
    .use(connect.logger('dev'))
    .use(connect.static(staticDir));
  if(connectCompiler){
    console.log('Will compile assets with connect-assets.');
    var ignore = [];
    var quote = function(str) {
      return (str+'').replace(/([.?*+^$[\]\\(){}|-])/g, "\\$1");
    }
    for(var prefix in proxyPaths){
      ignore.push(quote(prefix));
    }
    if(ignore.length){
      //console.log("Asset compile ignore paths:", '^(' +  ignore.join('|') + ')');
      ignore  = new RegExp('^(' +  ignore.join('|') + ')');
    } else {
      ignore = null;
    }
    app.use(connectCompiler({
      enabled: ['coffee'] ,
      src: staticDir,
      dest: tmpDir,
      ignore :  ignore,
    }))
    .use(connect.static(tmpDir));
  }
  app.use(function(req, res, next) {
      for(var prefix in proxyPaths){
        var dbPath = proxyPaths[prefix];
        if (req.url.indexOf(prefix) === 0) {
          var dbURL = req.url.replace(prefix,dbPath);
          if(logDbRewrites){
            console.log("*** ", req.url , ' -> ', dbURL);
          }
          req.url = dbURL;
          req.headers['host'] = dbUrlObj.host;
          proxy.proxyRequest(req, res);
          return;
        }
      }
      var body = '404 Not found.\nNo static file or db route matched.';
      res.statusCode = 404;
      res.setHeader('Content-Length', body.length);
      res.end(body);
     })
    .use(connect.errorHandler())
    .listen(port);
  console.log("Serving couchapp at: http://0.0.0.0:" + port +"/");

  //Cleanup the temp directory on exit
  var cleanup = function(){
    var rmDir = function(dirPath) {
      try { var files = fs.readdirSync(dirPath); }
      catch(e) { return; }
      if (files.length > 0)
        for (var i = 0; i < files.length; i++) {
          var filePath = dirPath + '/' + files[i];
          if (fs.statSync(filePath).isFile())
            fs.unlinkSync(filePath);
          else
            rmDir(filePath);
        }
      fs.rmdirSync(dirPath);
    };
    rmDir(tmpDir);
  };
  process.on('exit',cleanup);
  process.on('SIGINT',function(){
    cleanup()
    process.exit();
  });
}

exports.boilerDirectory = path.join(__dirname, 'boiler')

