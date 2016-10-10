var path = require('path')
  , fs = require('fs')
  , watch = require('watch')
  , request = require('request')
  , crypto = require('crypto')
  , mimetypes = require('./mimetypes')
  , spawn = require('child_process').spawn
  ;

var h = {'content-type':'application/json', 'accept-type':'application/json'}
  
/**
 * Recursively load directory contents into ddoc
 *
 * It's really convenient to see the main couchapp code in single file,
 * rather than mapped into little files in lots of directories like
 * the python couchapp. But there are definitely cases where we might want 
 * to use some module or another on the server side. This addition
 * loads file contents from a given directory (recursively) into a js 
 * object that can be added to a design document and require()'d in 
 * lists, shows, etc. 
 *
 * Use couchapp.loadFiles() in app.js like this:
 *
 *    ddoc = {
 *        _id: '_design/app'
 *      , views: {}
 *      , ...
 *      , lib: couchapp.loadFiles('./lib')
 *      , vendor: couchapp.loadFiles('./vendor')
 *    }
 *
 * Optionally, pass in operators to process file contents. For example, 
 * generate mustache templates from jade templates.
 *
 * In yourapp/templates/index.jade
 *  
 * !!!5
 * html
 *   head
 *     //- jade locals.title
 *     title!= title
 *   body
 *     .item
 *       //- mustache variable for server-side rendering
 *       h1 {{ heading }}
 *
 * in yourapp/app.js
 * var couchapp = require('couchapp')
 *   , jade = require('jade')
 *   , options = {
 *       , operators: [
 *           function renderJade (content, options) {
 *             var compiler = jade.compile(content);
 *             return compiler(options.locals || {});
 *           }
 *         ]
 *       , locals: { title: 'Now we\'re cookin with gas!' }
 *   };
 *
 * ddoc = { ... };
 * 
 * ddoc.templates = loadFiles(dir, options);
 */

function loadFiles(dir, options) {
  var listings = fs.readdirSync(dir)
    , options = options || {}
    , obj = {};

  listings.forEach(function (listing) {
    var file = path.join(dir, listing)
      , prop = listing.split('.')[0] // probably want regexp or something more robust
      , stat = fs.statSync(file);

      if (stat.isFile()) { 
        var content = fs.readFileSync(file).toString();
        if (options.operators) {
          options.operators.forEach(function (op) {
            content = op(content, options);
          });
        }
        obj[prop] = content;
      } else if (stat.isDirectory()) {
        obj[listing] = loadFiles(file, options);
      }
  });

  return obj;
}

/**
 * End of patch (also see exports and end of file)
 */

function loadAttachments (doc, root, prefix) {
  doc.__attachments = doc.__attachments || []
  try {
    fs.statSync(root)
  } catch(e) {
    throw e
    throw new Error("Cannot stat file "+root)
  }
  doc.__attachments.push({root:root, prefix:prefix});
}

function copy (obj) {
  var n = {}
  for (i in obj) n[i] = obj[i];
  return n
}

  
function createApp (doc, url, cb) {
  var app = {doc:doc}
  
  app.fds = {};
  
  app.prepare = function () {
    var p = function (x) {
      for (i in x) {
        if (i[0] != '_') {
          if (typeof x[i] == 'function') {
            x[i] = x[i].toString()
            x[i] = 'function '+x[i].slice(x[i].indexOf('('))
          }
          if (typeof x[i] == 'object') {
            p(x[i])
          }
        }
      }
    }
    p(app.doc);
    app.doc.__attachments = app.doc.__attachments || []
    app.doc.attachments_md5 = app.doc.attachments_md5 || {}
    app.doc._attachments = app.doc._attachments || {}
  }
  
  var push = function (callback) {
    console.log('Serializing.')
    var doc = copy(app.doc);
    doc._attachments = copy(app.doc._attachments)
    delete doc.__attachments;
    var body = JSON.stringify(doc)
    console.log('PUT '+url.replace(/^(https?:\/\/[^@:]+):[^@]+@/, '$1:******@'))
    request({uri:url, method:'PUT', body:body, headers:h}, function (err, resp, body) {
      if (err) throw err;
      if (resp.statusCode !== 201) {
        throw new Error("Could not push document\nCode: " + resp.statusCode + "\n"+body);
      }
      app.doc._rev = JSON.parse(body).rev
      console.log('Finished push. '+app.doc._rev)
      request({uri:url, headers:h}, function (err, resp, body) {
        body = JSON.parse(body);
        app.doc._attachments = body._attachments;
        if (callback) callback()
      })
    })
  }
  
  app.push = function (callback) {
    var revpos
      , pending_dirs = 0
      ;
    
    console.log('Preparing.')
    var doc = app.current;
    for (i in app.doc) {
      if (i !== '_rev') doc[i] = app.doc[i]
    }
    app.doc = doc;
    app.prepare();
    revpos = app.doc._rev ? parseInt(app.doc._rev.slice(0,app.doc._rev.indexOf('-'))) : 0;
    
    var coffeeCompile;
    var coffeeExt;
    try{
      coffeeCompile = require('coffee-script');
      coffeeExt = /\.(lit)?coffee$/;
    } catch(e){}

    app.doc.__attachments.forEach(function (att) {
      watch.walk(att.root, {ignoreDotFiles:true}, function (err, files) {
        pending_dirs += 1;
        var pending_files = Object.keys(files).length;
        for (i in files) { (function (f) {
          fs.readFile(f, function (err, data) {
            if(f.match(coffeeExt)){
              data = new Buffer( coffeeCompile.compile(data.toString()) );
              f = f.replace(coffeeExt,'.js');
            }
            f = f.replace(att.root, att.prefix || '').replace(/\\/g,"/");
            if (f[0] == '/') f = f.slice(1)
            if (!err) {
              var d = data.toString('base64')
                , md5 = crypto.createHash('md5')
                , mime = mimetypes.lookup(path.extname(f).slice(1))
                ;
              md5.update(d)
              md5 = md5.digest('hex')
              if (app.doc.attachments_md5[f] && app.doc._attachments[f]) {
                if (app.doc._attachments[f].revpos === app.doc.attachments_md5[f].revpos &&
                    app.doc.attachments_md5[f].md5 === md5) {
                  pending_files -= 1;
                  if(pending_files === 0){
                    pending_dirs -= 1;
                    if(pending_dirs === 0){
                      push(callback);
                    }
                  }
                  return; // Does not need to be updated.
                }
              }
              app.doc._attachments[f] = {data:d, content_type:mime};
              app.doc.attachments_md5[f] = {revpos:revpos + 1, md5:md5};
            }
            pending_files -= 1
            if(pending_files === 0){
              pending_dirs -= 1;
              if(pending_dirs === 0){
                push(callback);
              }
            }
          })
        })(i)}
      })
    })
    if (!app.doc.__attachments || app.doc.__attachments.length == 0) push(callback);
  }  
  
  app.sync = function (callback) {
    // A few notes.
    //   File change events are stored in an array and bundled up in to one write call., 
    // this reduces the amount of unnecessary processing as we get a lof of change events.
    //   The file descriptors are stored and re-used because it cuts down on the number of bad change events.
    //   And finally, we check the md5 and only push when the document is actually been changed.
    //   A lot of crazy workarounds for the fact that we basically get an event every time someone
    // looks funny at the underlying files and even reading and opening fds to check on the file trigger
    // more events.
    
    app.push(function () {
      var changes = [];
      console.log('Watching files for changes...')
      app.doc.__attachments.forEach(function (att) {
        var pre = att.root
        var slash = (process.platform === 'win32') ? '\\' : '/';
        if (pre[pre.length - 1] !== slash) pre += slash;
        watch.createMonitor(att.root, {ignoreDotFiles:true}, function (monitor) {
          monitor.on("removed", function (f, stat) {
            f = f.replace(pre, '');
            changes.push([null, f]);
          })
          monitor.on("created", function (f, stat) {
            changes.push([f, f.replace(pre, ''), stat]);
          })
          monitor.on("changed", function (f, curr, prev) {
            changes.push([f, f.replace(pre, ''), curr]);
          })
        })
      })
      var check = function () {
        var pending = 0
          , revpos = parseInt(app.doc._rev.slice(0,app.doc._rev.indexOf('-')))
          , dirty = false
          ;
        if (changes.length > 0) {
          changes.forEach(function (change) {
            if (!change[0]) {
              delete app.doc._attachments[change[1]];
              dirty = true;
              console.log("Removed "+change[1]);
            } else {
              pending += 1
              
              fs.readFile(change[0], function (err, data) {
                var f = change[1]
                  , d = data.toString('base64')
                  , md5 = crypto.createHash('md5')
                  , mime = mimetypes.lookup(path.extname(f).slice(1))
                  ;

                md5.update(d)
                md5 = md5.digest('hex')
                pending -= 1
                if (!app.doc.attachments_md5[f] || (md5 !== app.doc.attachments_md5[f].md5) ) {
                  app.doc._attachments[f] = {data:d, content_type:mime};
                  app.doc.attachments_md5[f] = {revpos:revpos + 1, md5:md5};
                  dirty = true;
                  console.log("Changed "+change[0]);
                }
                if (pending == 0 && dirty) push(function () {dirty = false; setTimeout(check, 50)})
                else if (pending == 0 && !dirty) setTimeout(check, 50)
                
              })
            }
            
          })
          changes = []
          if (pending == 0 && dirty) push(function () {dirty = false; setTimeout(check, 50)})
          else if (pending == 0 && !dirty) setTimeout(check, 50)
        } else {
          setTimeout(check, 50);
        }
      }
      setTimeout(check, 50)
    })
  }
  var _id = doc.app ? doc.app._id : doc._id
  
  if (url.slice(url.length - _id.length) !== _id) url += '/' + _id;

  request({uri:url, headers:h}, function (err, resp, body) {
    if (err) throw err;
    if (resp.statusCode == 404) app.current = {};
    else if (resp.statusCode !== 200) throw new Error("Failed to get doc\n"+body)
    else app.current = JSON.parse(body)
    cb(app)
  })
}

exports.createApp = createApp
exports.loadAttachments = loadAttachments
exports.bin = require('./bin')
exports.loadFiles = loadFiles
