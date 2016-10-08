var couchapp = require('couchapp')
  , path = require('path')


ddoc =
  { _id:'_design/ui'
  , rewrites :
    [ {from:"/", to:'index.html'}
    , {from:"/favicon.ico", to:'../../npm/favicon.ico'}
    , {from:"/api", to:'../../'}
    , {from:"/api/*", to:'../../*'}
    , {from:"/*", to:'*'}
    ]
  , lists : {}
  }


function packageSearch (doc) {
  var descriptionBlacklist =
    [ "for"
    , "and"
    , "in"
    , "are"
    , "is"
    , "it"
    , "do"
    , "of"
    , "on"
    , "the"
    , "to"
    , "as"
    ]

  if (doc.name) { // There aren't any better attributes for check if isPackage()
    if (doc.name) {
      var names = [doc.name];
      if (doc.name.indexOf('-') !== -1) doc.name.split('-').forEach(function (n) {names.push(n)});
      if (doc.name.indexOf('_') !== -1) doc.name.split('_').forEach(function (n) {names.push(n)});
      names.forEach(function (n) {
        if (n.length > 1) emit(n.toLowerCase(), doc);
      });
    }
    if (doc['dist-tags'] && doc['dist-tags'].latest && (
        doc.versions[doc['dist-tags'].latest].keywords || doc.versions[doc['dist-tags'].latest].tags
        )) {
      var tags = (doc.versions[doc['dist-tags'].latest].keywords || doc.versions[doc['dist-tags'].latest].tags)
      tags.forEach(function (tag) {
        tag.split(' ').forEach(function (t) {
          if (t.length > 0) emit(t.toLowerCase(), doc);
        });
      })
    }
    if (doc.description) {
      doc.description.split(' ').forEach(function (d) {
        d = d.toLowerCase();
        while (d.indexOf('.') !== -1) d = d.replace('.', '');
        while (d.indexOf('\n') !== -1) d = d.replace('\n', '');
        while (d.indexOf('\r') !== -1) d = d.replace('\n', '');
        while (d.indexOf('`') !== -1) d = d.replace('`', '');
        while (d.indexOf('_') !== -1) d = d.replace('_', '');
        while (d.indexOf('"') !== -1) d = d.replace('"', '');
        while (d.indexOf('\'') !== -1) d = d.replace('\'', '');
        while (d.indexOf('(') !== -1) d = d.replace('(', '');
        while (d.indexOf(')') !== -1) d = d.replace(')', '');
        while (d.indexOf('[') !== -1) d = d.replace('[', '');
        while (d.indexOf(']') !== -1) d = d.replace(']', '');
        while (d.indexOf('{') !== -1) d = d.replace('{', '');
        while (d.indexOf('}') !== -1) d = d.replace('}', '');
        while (d.indexOf('*') !== -1) d = d.replace('*', '');
        while (d.indexOf('%') !== -1) d = d.replace('%', '');
        while (d.indexOf('+') !== -1) d = d.replace('+', '');
        if (descriptionBlacklist.indexOf(d) !== -1) d = '';
        if (d.length > 1) emit(d, doc);
      })
    }
  }
}

function dependencies (doc) {
  if (doc['dist-tags'] && doc['dist-tags'].latest) {
    var dist = doc.versions[doc['dist-tags'].latest];
    for (i in dist.dependencies) {
      emit(i, dist.dependencies[i])
    }
  }
}

ddoc.views =
  { search: { map: packageSearch }
  , dependencies: {map: dependencies, reduce:"_count"}
  , updated: {map: function (doc) {
      var l = doc["dist-tags"].latest
        , t = doc.time && doc.time[l]
      if (t) emit(t, 1)
    }}
  , tags:
    { map: function (doc) {
             if (doc['dist-tags'] && doc['dist-tags'].latest) {
              doc.versions[doc['dist-tags'].latest].tags.forEach(function (t) {emit(t, 1)})
             }
           }
    , reduce: "_sum"
    }
  , author:
    { map: function (doc) {
             if (doc.author && doc.author.name) {
               emit(doc.author.name, 1);
             }
           }
    , reduce: "_sum"
    }
  , analytics:
    { map: function (doc) {
             if (doc.time) {
               if (doc.time.modified) {
                 emit(['latest', doc.time.modified], 1);
               }
               if (doc.time.created) {
                 emit(['created', doc.time.created], 1);
               }
               for (i in doc.time) {
                 emit(['update', doc.time[i]], 1);
               }
             }
           }
    , reduce: "_sum"
    }
  }
  ;
  
ddoc.lists.dependencies_limit = function(head, req) {
    var deps = [];
    while(row = getRow()) {
        deps.push(row);
    }
    var sorted = deps.sort(function(a,b) { return req.query.descending !== "true" ? a.value - b.value : b.value - a.value; });    
    // using list_Limit rather than limit because limit appears to limit the initial view set
    // assuming there's a supported convention but using this for now
    var limit = req.query.list_limit && parseInt(req.query.list_limit);
    send(JSON.stringify({ total_rows: deps.length, rows: limit ? sorted.splice(0, limit) : sorted}));
};

ddoc.lists.search = function(head, req) {
    Object.keys = Object.keys
      || function (o) { var a = []
                        for (var i in o) a.push(i)
                        return a }
    var set = {};
    var rows = [];
    while(row = getRow()) {
        set[row.id] = { key: row.id, count: set[row.id] ? set[row.id].count + 1 : 1, value: row.value };
    }
    var keys = Object.keys(set);
    for(var i=0; i<keys.length; i++) {
        rows.push(set[keys[i]]);
    }
    send(JSON.stringify({ rows: rows} ));
};


// ddoc.validate_doc_update = function (newDoc, oldDoc, userCtx) {
//   if (newDoc._deleted === true && userCtx.roles.indexOf('_admin') === -1) {
//     throw "Only admin can delete documents on this database.";
//   }
// }

couchapp.loadAttachments(ddoc, path.join(__dirname, 'attachments'));

module.exports = ddoc;
