if (!Object.keys) {
  Object.keys = function (obj) {
    var keys = [];
    for (i in obj) keys.push(i);
    return keys;
  }
}
if (!Array.prototype.forEach) {
  Array.prototype.forEach = function (cb) {
    for (var i=0;i<this.length;i++) {
      cb(this[i]);
    }
  }
}


// escape functionality taken from https://github.com/natevw/flatstache.js/
var _re1 = /[&\"'<>\\]/g;
var escape_map = {"&": "&amp;", "\\": "&#92;", "\"": "&quot;", "'": "&#39;", "<": "&lt;", ">": "&gt;"};
var escapeHTML = function(s) {
    if (!s) return s;
    return s.toString().replace(_re1, function(c) { return escape_map[c]; });
}

var request = function (options, callback) {
  options.success = function (obj) {
    callback(null, obj);
  }
  options.error = function (err) {
    if (err) callback(err);
    else callback(true);
  }
  if (options.data && typeof options.data == 'object') {
    options.data = JSON.stringify(options.data)
  }
  if (!options.dataType) options.processData = false;
  if (!options.dataType) options.contentType = 'application/json';
  if (!options.dataType) options.dataType = 'json';
  $.ajax(options)
}

function prettyDate(time) {
  if (time.indexOf('.') !== -1) time = time.slice(0, time.indexOf('.'))+'Z'
  var date = new Date((time || "").replace(/-/g,"/").replace(/[TZ]/g," ")),
      date = new Date(date.getTime() - (date.getTimezoneOffset() * 1000 * 60))
      diff = (((new Date()).getTime() - date.getTime()) / 1000),
      day_diff = Math.floor(diff / 86400)
      ;
  
  if (day_diff === -1) return "now"
  if ( day_diff >= 31) return day_diff + ' days ago';
  if ( isNaN(day_diff) || day_diff < 0 || day_diff >= 31 ) return;
  
  return day_diff == 0 && (
      diff < 60 && "just now" ||
      diff < 120 && "1 minute ago" ||
      diff < 3600 && Math.floor( diff / 60 ) + " minutes ago" ||
      diff < 7200 && "1 hour ago" ||
      diff < 86400 && Math.floor( diff / 3600 ) + " hours ago") ||
    day_diff == 1 && "Yesterday" ||
    day_diff < 7 && day_diff + " days ago" ||
    day_diff < 31 && Math.ceil( day_diff / 7 ) + " weeks ago";
}

function isGithubUrl(url) {
  return url.slice(0, 'http://github.com'.length) === 'http://github.com' ||
         url.slice(0, 'https://github.com'.length) === 'https://github.com' ||
         url.slice(0, 'git://github.com'.length) === 'git://github.com';
}

$.expr[":"].exactly = function(obj, index, meta, stack){ 
  return ($(obj).text() == meta[3])
}

var param = function( a ) {
  // Query param builder from jQuery, had to copy out to remove conversion of spaces to +
  // This is important when converting datastructures to querystrings to send to CouchDB.
  var s = [];
  if ( jQuery.isArray(a) || a.jquery ) {
    jQuery.each( a, function() { add( this.name, this.value ); });    
  } else { 
    for ( var prefix in a ) { buildParams( prefix, a[prefix] ); }
  }
  return s.join("&");
  function buildParams( prefix, obj ) {
    if ( jQuery.isArray(obj) ) {
      jQuery.each( obj, function( i, v ) {
        if (  /\[\]$/.test( prefix ) ) { add( prefix, v );
        } else { buildParams( prefix + "[" + ( typeof v === "object" || jQuery.isArray(v) ? i : "") +"]", v )}
      });        
    } else if (  obj != null && typeof obj === "object" ) {
      jQuery.each( obj, function( k, v ) { buildParams( prefix + "[" + k + "]", v ); });        
    } else { add( prefix, obj ); }
  }
  function add( key, value ) {
    value = jQuery.isFunction(value) ? value() : value;
    s[ s.length ] = encodeURIComponent(key) + "=" + encodeURIComponent(value);
  }
}

function clearContent () {
  $('div#content').html('')
  $('div#totals').html('')
}

var app = {};
app.index = function () {
  var currentTerms = []
    , searchResults = {}
    , docs = {}
    , currentSearch = ''
    , lastSearchForPage = ''
    , limit = 15
    ;
  clearContent();
  $('div#content').html(
    '<div id="search-box">' +
      '<div id="search-box-title">Find packages...</div>' +
      '<div id="search-box-input">' +
        '<input id="search-input"></input>' +
        '<span class="browse-link">or <a href="/#/_browse/all">browse packages</a>.</span>' +
      '</div>' +
    '</div>' +
    '<div id="main-container">' +
      '<div id="results"></div>' +
      '<div class="spacer"></div>' +
      '<div id="top-packages">' +
        '<div id="latest-packages"><div class="top-title">Latest Updates</div></div>' +
        '<div id="top-dep-packages"><div class="top-title">Most Depended On</div></div>' +
      '</div>' +
      '<div class="spacer"></div>' +
    '</div>'
  )
  
  request({url:'/api/_all_docs?limit=0'}, function (err, resp) {
    $('div#totals').html('<a href="/#/_browse/all">' + (resp.total_rows - 1) +' total packages</a>')
  })
  
  request({url:'/_view/updated?descending=true&limit='+limit+'&include_docs=false'}, function (err, resp) {
    resp.rows.forEach(function (row) {
      $('<div class="top-package"></div>')
      .append('<div class="top-package-title"><a href="#/'+row.id+'">'+row.id+'</a></div>')
      .append('<div class="top-package-updated">'+prettyDate(row.key) +'</div>')
      .append('<div class="spacer"></div>')
      .appendTo('div#latest-packages')
    })
  })
  
  request({url:'/_list/dependencies_limit/dependencies?group=true&descending=true&list_limit='+limit}, function (err, resp) {
    var results = {};
    resp.rows.forEach(function (row) {
        $('<div class="top-package"></div>')
        .append('<div class="top-package-title"><a href="#/'+escapeHTML(row.key)+'">'+escapeHTML(row.key)+'</a></div>')
        .append('<div class="top-package-dep">'+escapeHTML(row.value)+'</div>')
        .append('<div class="spacer"></div>')
        .appendTo('div#top-dep-packages')
    })
  })
    
  var updateResults = function () {
    currentSearch = $('input#search-input').val().toLowerCase();
    currentTerms = $.trim(currentSearch).split(' ');
    if (lastSearchForPage === currentSearch) return;
    if (currentSearch == '') $('div#top-packages').show();
    else $('div#top-packages').hide();
    var docsInPage = {}
      , ranked = {}
      ;
    currentTerms.forEach(function (term) {
      if (searchResults[term] && searchResults[term] !== 'pending') {
        searchResults[term].forEach(function (id) {
          if (docs[id] !== 'pending') docsInPage[id] = docs[id]
        });
      }
    })
    for (i in docsInPage) {
      var doc = docsInPage[i];
      doc.rank = 0
      doc.tagsInSearch = [];
      if (doc.description) {
        doc.htmlDescription = doc.description.split('&').join('&amp;')
                                             .split('"').join('&quot;')
                                             .split('<').join('&lt;')
                                             .split('>').join('&gt;')
      }
      
      if (doc._id.toLowerCase() === currentSearch) doc.rank += 1000      
      
      if (doc['dist-tags'] && doc['dist-tags'].latest) {
        var tags = doc.versions[doc['dist-tags'].latest].keywords || doc.versions[doc['dist-tags'].latest].tags || [];
      } else { 
        var tags = [];
      }
      
      tags = tags.map(function (tag) {
          return tag.split('&').join('&amp;')
                    .split('"').join('&quot;')
                    .split('<').join('&lt;')
                    .split('>').join('&gt;')
      })
      currentTerms.forEach(function (t) {
        t = t.toLowerCase();
        if (doc._id.toLowerCase().indexOf(t.toLowerCase()) !== -1) doc.rank += 750;
        if (tags.indexOf(t) !== -1) {
          doc.rank += 300;
          doc.tagsInSearch.push(t);
        }
        if (doc.description && doc.description.toLowerCase().indexOf(t) !== -1) {
          doc.rank += 100;
          var i = 0;
          while (doc.htmlDescription.toLowerCase().indexOf(t, i) !== -1) {
            var i = doc.htmlDescription.toLowerCase().indexOf(t, i);
            doc.htmlDescription = 
                                ( doc.htmlDescription.slice(0, i) 
                                + '<span class="desc-term">'
                                + doc.htmlDescription.slice(i, i+t.length)
                                + '</span>'
                                + doc.htmlDescription.slice(i + t.length)
                                )
                                ;
            i = i + t.length + '<span class="desc-term"></span>'.length
          }
          
        }
        doc.tags = tags;
      })
      
      if (!ranked[doc.rank]) ranked[doc.rank] = [];
      ranked[doc.rank].push(doc);
    }
    
    $('div#results').html('');
    var keys = Object.keys(ranked);
    for (var i=0;i<keys.length;i++) keys[i] = parseInt(keys[i])
    keys.sort(function(a,b){return a - b;});
    keys.reverse();
    if (keys.length === 0) {
      $('div#results').html('<div>No Results</div>')
    }
    keys.forEach(function (i) { ranked[i].forEach(function (doc) {
      var result = $(
        '<div class="result-container">' +
          '<div class="result">' + 
            '<span class="result-name"><a href="#/'+doc._id+'">'+doc._id+'</a></span>' + 
            '<span class="result-desc">'+(doc.htmlDescription || '') + '</span>' +
            '<div class="result-tags"></div>' +
            '<div class="spacer"></div>' +
          '</div>' +
        '</div>' +
        '<div class="spacer"></div>'
      )
      
      if (doc.tags.length > 0) {
        doc.tags.forEach(function (tag) {
          result.find('div.result-tags').append('<span class="tag">'+tag+'</span>')
        })
      }
      
      result.appendTo('div#results')
      $('span.tag').click(function () {
        $('input#search-input').val($(this).text()).change();
      })
    })})
    
    lastSearchForPage = currentSearch;
  }  
  
  var hcTimer = null;
  var handleChange = function () {
    if (hcTimer) clearTimeout(hcTimer);
    hcTimer = setTimeout(handleChange_, 100);
  }
  function handleChange_ () {
    currentSearch = $('input#search-input').val().toLowerCase();
    currentTerms = $.trim(currentSearch).split(' ')
    if (currentSearch === '') {
      $('div#results').html('')
      $('div#top-packages').show();
    }
    lastSearchForPage = ''
    var terms = currentTerms
      , c = currentSearch
      , tlength = terms.length
      ;

    terms.forEach(function (term) {
      if (!searchResults[term]) {
        searchResults[term] = 'pending'
        var qs = param(
          { startkey: JSON.stringify(term)
          , endkey: JSON.stringify(term+'ZZZZZZZZZZZZZZZZZZZ')
          , limit:25
          }
        )
        ;
        request({url:'/_list/search/search?'+qs}, function (err, resp) {
          var docids = [];
          searchResults[term] = [];
          resp.rows.forEach(function (row) {
            searchResults[term].push(row.key);
            row.value.name = row.value.name.toLowerCase();
            docs[row.key] = row.value;
            updateResults();
          })
          if (docids.length === 0) {
            lastSearchForPage = '';
            updateResults();
            return 
          }
          
        })
      } else {tlength -= 1}
    })
    if (tlength == 0) {lastSearchForPage = ''; updateResults()}
  }
  
  $('input#search-input').change(handleChange);
  $('input#search-input').keyup(handleChange)
  $("input#search-input").focus();
};

app.showPackage = function () {
  var id = this.params.id;
  clearContent();
  $('div#content').html('<div id="main-container"></div>')
  request({url:'/api/'+id}, function (err, doc) {
    var package = $('div#main-container')
    
    package.append('<div class="package-title">'+doc._id+'</div>')
    // package.append('<div class="package-description">'+doc.description+'</div>')
    
    if (doc['dist-tags'] && doc['dist-tags'].latest) {
      if (doc.versions[doc['dist-tags'].latest].homepage) {
        package.append('<div class="pkg-link"><a href="'+escapeHTML(doc.versions[doc['dist-tags'].latest].homepage)+'">'+escapeHTML(doc.versions[doc['dist-tags'].latest].homepage)+'</a>')
      }
    }
    
    if (typeof doc.repository === 'string') {
      repositoryUrl = doc.repository;
      doc.repository = {
        type: (isGithubUrl(repositoryUrl) ? 'git' : 'unknown'),
        url: repositoryUrl
      }
    }
    if (doc.repository && doc.repository.type === 'git' && isGithubUrl(doc.repository.url) ) {
          package.append('<div class="pkg-link"><a class="github" href="' + escapeHTML(doc.repository.url.replace('.git', '').replace('git://', 'https://')) + '">github</a></div>')
    }
     
    
    package.append('<div class="spacer"></div>')
    if (doc.time && doc.time.modified) {
      package.append('<div class="last-updated">Last updated: '+prettyDate(doc.time.modified)+'</div>')
    }
    if (doc.author && doc.author.name) {
      package.append('<div class="author">by: <a href="/#/_author/'+encodeURIComponent(doc.author.name)+'">'+doc.author.name+'</div>')
    }
    

    // 
    // if (doc.maintainers && doc.maintainers.length > 0) {
    //   var maintainers = $('<div class="package-maintainers"></div>').appendTo(package);
    //   doc.maintainers.forEach(function (m) {
    //     maintainers.append('<div class="package-maintainer">maintainer: '+m.name+'   </div>')
    //   })
    // }
    
    package.append(
      '<div id="versions-container">' + 
        '<div id="version-list"></div>' + 
        '<div id="version-info"></div>' +
        '<div class="spacer"></div>' +
      '</div>'
    )
    
    var showVersion = function (version) {
      var v = doc.versions[version];
      
      if (v.description) {
        v.htmlDescription = v.description.split('&').join('&amp;')
                                             .split('"').join('&quot;')
                                             .split('<').join('&lt;')
                                             .split('>').join('&gt;')
      } else {
        v.htmlDescription = ""
      }
      
      $('div#version-info').html(
        '<div class="version-info-cell">' +
          '<div class="version-info-key">Description</div>' +
          '<div class="version-info-value">'+v.htmlDescription+'</div>' +
        '</div>' + 
        '<div class="spacer"></div>' +
        '<div class="version-info-cell">' +
          '<div class="version-info-key">Version</div>' +
          '<div class="version-info-value">'+v.version+'</div>' +
        '</div>' + 
        '<div class="spacer"></div>'
      );
      
      if (doc.time && doc.time[version]) {
        $('div#version-info').append(
          '<div class="version-info-cell">' +
            '<div class="version-info-key">Published</div>' +
            '<div class="version-info-value">' + prettyDate(doc.time[version]) + '</div>' +
          '</div>' +
          '<div class="spacer"></div>'
        )
      }

      if (v.tags) {
        var h = '[ ';
        v.tags.forEach(function (tag) {
          if (tag !== v.tags[0]) h += ', '
          h += ('<a href="/#/_tags/'+escapeHTML(tag)+'">'+escapeHTML(tag)+'</a>')
        })
        h += ' ]'
        $('div#version-info').append(
          '<div class="version-info-cell">' +
            '<div class="version-info-key">Tags</div>' +
            '<div class="version-info-value">' + h + '</div>' +
          '</div>' +
          '<div class="spacer"></div>' 
        )
      }
      
      if (v.dependencies) {
        var h = ''
        for (i in v.dependencies) {
          h += '<a class="dep-link" href="#/'+escapeHTML(i)+'">'+escapeHTML(i)+'</a> '
        }
        $('div#version-info').append('<div class="version-info-cell">' +
            '<div class="version-info-key">Dependencies</div>' +
            '<div class="version-info-value">' + h + '</div>' +
          '</div>' +
          '<div class="spacer"></div>'
        )
      }
      
      if (v.homepage) {
        $('div#version-info').append(
          '<div class="version-info-cell">' +
            '<div class="version-info-key">Homepage</div>' +
            '<div class="version-info-value">' + escapeHTML(v.homepage) + '</div>' +
          '</div>' +
          '<div class="spacer"></div>'
        )
      }
      if (v.repository) {
        if (typeof v.repository === 'string') {
          repositoryUrl = v.repository;
          v.repository = {
            type: (isGithubUrl(repositoryUrl) ? 'git' : 'unknown'),
            url: repositoryUrl
          };
        }
        $('div#version-info').append(
          '<div class="version-info-cell">' +
            '<div class="version-info-key">Repository</div>' +
            '<div class="version-info-value">' + 
              escapeHTML(v.repository.type) + ':    <a href="' + escapeHTML(v.repository.url) + '">'+ escapeHTML(v.repository.url) + '</a>' +
            '</div>' +
          '</div>' +
          '<div class="spacer"></div>'
        )
      }
      if (v.bugs) {
        var bugs = $(
          '<div class="version-info-cell">' +
            '<div class="version-info-key">Bugs</div>' +
            '<div class="version-info-value"></div>' +
          '</div>' +
          '<div class="spacer"></div>'
        )
        var bugsHtml = ''
        if (v.bugs.email) {
          bugsHtml+= '<div>email:    ' + '<a href="mailto='+v.bugs.email+'">'+v.bugs.email+'</a></div>'
        }
        if (v.bugs.url) {
          bugsHtml += '<div>url:    ' + '<a href="'+v.bugs.url+'">'+v.bugs.url+'</a></div>'
        }
        bugs.find('div.version-info-value').html(bugsHtml)
        $('div#version-info').append(bugs)
      }
      if (v.engines) {
      var eng = [];
        for (i in v.engines) { eng.push( escapeHTML(i) + ' (' + escapeHTML(v.engines[i]) + ')' ); }
        $(
          '<div class="version-info-cell">' +
            '<div class="version-info-key">Engines</div>' +
            '<div class="version-info-value">'+eng.join(', ')+'</div>' +
          '</div>' +
          '<div class="spacer"></div>'
        ).appendTo('div#version-info');
      }
      if (v.licenses) {
      h = '';
        for (i in v.licenses) {
          h += '<a href="'+escapeHTML(v.licenses[i].url)+'">'+escapeHTML(v.licenses[i].type)+'</a>';
        }
        $(
          '<div class="version-info-cell">' +
            '<div class="version-info-key">Licenses</div>' +
            '<div class="version-info-value">'+h+'</div>' +
          '</div>' +
          '<div class="spacer"></div>'
        ).appendTo('div#version-info');
      }

      
      //  +
      // '<div class="version-info-cell">' +
      //   '<span class="version-info-key">Author</span>' +
      //   '<span class="version-info-value">'+v.htmlDescription+'<span>' +
      // '</div>' +
      // '<div class="version-info-cell">' +
      //   '<span class="version-info-key">Repository</span>' +
      //   '<span class="version-info-value">'+v.htmlDescription+'<span>' +
      // '</div>' +
      
    }
    showVersion(doc['dist-tags'].latest);
    
    if (doc['dist-tags']) {
      for (i in doc['dist-tags']) {
        $('<div class="package-download">' +
            '<div id="'+doc['dist-tags'][i]+'" class="version-link">'+
              '<a href="' + doc.versions[doc['dist-tags'][i]].dist.tarball.replace('jsregistry:5984', 'registry.npmjs.org').replace('packages:5984', 'registry.npmjs.org')+'">'+i+'</a>  ('+doc.versions[doc['dist-tags'][i]].version+')' + 
            '</div>' +
          '</div>')
          .addClass('version-selected')
          .appendTo('div#version-list')
          ;
      }
      package.append('<br/>')
    }
    
    if (doc.versions) {
      var versions = Object.keys(doc.versions);
      versions.reverse();
      versions.forEach(function (i) {
        $('div#version-list').append(
          '<div class="package-download">' +
            '<div id="'+i+'" class="version-link">'+
              '<a href="'+doc.versions[i].dist.tarball.replace('jsregistry:5984', 'registry.npmjs.org').replace('packages:5984', 'registry.npmjs.org')+'">'+i+'</a>' + 
            '</div>' +
          '</div>'
        )
      })
    }
    
    $('div.version-link').mouseover(function () {
      $('div.version-selected').removeClass('version-selected');
      $(this).parent().addClass('version-selected');
      $('div#version-info').css(
        { top: $(this).position().top - $(this).parent().parent().position().top 
        , position:'relative'
        })
      showVersion(this.id)
    })
    
    var usersStr = '<h4>People who starred '+id+'</h4><div class="users"><p>'
    if (doc.users)
      for (var usingUser in doc.users)
        if (doc.users[usingUser])
          usersStr += (usersStr.length?' ':'')+'<span class="user">'+usingUser.replace(/</g, '&lt;').replace(/>/g, '&gt;')+'</span>'
      usersStr += '</p></div>'
      package.append(usersStr)

    request({url:'/_view/dependencies?reduce=false&key="'+id+'"'}, function (e, resp) {
      if (resp.rows.length === 0) return;
      var deps = ''
      deps += '<h4>Packages that depend on '+id+'</h4><div class="dependencies"><p>'
      for (var i=0;i<resp.rows.length;i++) {
        deps += '<span class="dep"><a class="dep" href="/#/' +
                 resp.rows[i].id+'">'+resp.rows[i].id+'</a></span> '
      }
      deps += '</p></div>'
      package.append(deps)
    })
  })
}

app.browse = function () {
  var limit = 100
    ;
  clearContent();
  
  $(
    '<div id="browse-anchors">' +
      '<a href="/#/_browse/all">all</a>' +
      '<a href="/#/_browse/tags">tags</a>' +
      '<a href="/#/_browse/author">author</a>' +
      '<a href="/#/_browse/updated">updated</a>' +
      '<a href="/#/_browse/deps">depended on</a>' +
    '</div>'
  )
  .appendTo('div#content')
  
  var c = $('<div id="main-container"></div>')
    .appendTo('div#content')
    ;
  var fetch = function (url, cb) {
    $('div#more-all').remove();
    request({url:url(limit)}, function (err, resp) {
      cb(resp);
      limit += 100;
      $('<div id="more-all">Load 100 more</div>')
        .click(function () {fetch(url, cb);})
        .appendTo(c)
        ;
    })
  }
  
  var routes = {};
  routes.all = function () {
    fetch( 
      function (limit) {
        return '/api/_all_docs?include_docs=true&limit='+limit+'&skip='+(limit - 100) 
      }
      , function (r) {
        var h = ''
        r.rows.forEach(function (row) {
          if (!row.doc.description) row.doc.description = ""
          row.doc.htmlDescription = row.doc.description
                                    .split('&').join('&amp;')
                                    .split('"').join('&quot;')
                                    .split('<').join('&lt;')
                                    .split('>').join('&gt;')
          if (row.id[0] !== '_') {
            h += (
              '<div class="all-package">' + 
                '<div class="all-package-name"><a href="/#/'+row.id+'">' + row.id + '</a></div>' +
                '<div class="all-package-desc">' + row.doc.htmlDescription + '</div>' +
              '</div>' +
              '<div class="spacer"></div>'
            )
          }
        });
      c.append(h);
      }
    );
    $('a:exactly("all")').css('text-decoration', 'underline');
  }
  routes.tags = function () {
    request({url:'/_view/tags?group=true'}, function (e, resp) {
      resp.rows.forEach(function (row) {
        c.append(
          '<div class="all-package">' + 
            '<div class="all-package-name"><a href="/#/_tag/'+encodeURIComponent(row.key)+'">' + row.key + '</a></div>' +
            '<div class="all-package-desc">' + row.value + '</div>' +
          '</div>' +
          '<div class="spacer"></div>'
        )
      })
      
      request({url:'/_view/tags?reduce=false'}, function (e, resp) {
        resp.rows.forEach(function (row) {
          $('div.all-package-name:exactly("'+row.key+'")').next().append('<a href="/#/'+row.id+'" class="tag-val">'+row.id+'</a>')
        })
        $(self).remove();
      })
    })
    $('a:exactly("tags")').css('text-decoration', 'underline');
  }
  routes.author = function () {
    request({url:'/_view/author?group=true'}, function (e, resp) {
      resp.rows.forEach(function (row) {
        c.append(
          '<div class="all-package">' + 
            '<div class="all-package-author"><a href="/#/_author/'+encodeURIComponent(row.key)+'">' + row.key + '</a></div>' +
            '<div class="all-package-auth-list">' + row.value + '</div>' +
          '</div>' +
          '<div class="spacer"></div>'
        )
      })
      
      request({url:'/_view/author?reduce=false'}, function (e, resp) {
        resp.rows.forEach(function (row) {
          $('div.all-package-author:exactly("'+row.key+'")').next().append('<a href="/#/'+row.id+'" class="tag-val">'+row.id+' </a>')
        })
        $(self).remove();
      })
    })
    $('a:exactly("author")').css('text-decoration', 'underline');
  }
  routes.updated = function () {
    request({url:'/_view/updated'}, function (e, resp) {
      resp.rows.reverse();
      resp.rows.forEach(function (row) {
        c.append(
          '<div class="all-package">' + 
            '<div class="all-package-name">'+ prettyDate(row.key) +'</div>' +
            '<div class="all-package-value"><a href="/#/'+encodeURIComponent(row.id)+'">' + row.id + '</a></div>' +
          '</div>' +
          '<div class="spacer"></div>'
        )
      })
    })
    $('a:exactly("updated")').css('text-decoration', 'underline');
  }
  routes.deps = function () {
    request({url:'/_view/dependencies?group=true'}, function (e, resp) {
      var deps = {};
      resp.rows.forEach(function (row) {
        if (!deps[row.value]) deps[row.value] = []
        deps[row.value].push(row)
      })
      var keys = Object.keys(deps);
      keys.sort(function(a,b){return a - b;});
      keys.reverse();
      keys.forEach(function (k) {
        deps[k].forEach(function (row) {
          c.append(
            '<div class="all-package">' + 
              '<div class="all-package-deps"><a href="/#/'+encodeURIComponent(row.key)+'">' + escapeHTML(row.key) + '</a></div>' +
              '<div class="all-package-deps-value">'+escapeHTML(row.value)+'</div>' +
            '</div>' +
            '<div class="spacer"></div>'
          )
        })
      })
    })
    $('a:exactly("depended on")').css('text-decoration', 'underline');
  }
  if (this.params.view) routes[this.params.view]();
}
app.tags = function () {
  var tag = this.params.tag.split('&').join('&amp;')
                           .split('"').join('&quot;')
                           .split('<').join('&lt;')
                           .split('>').join('&gt;')
  clearContent();
  $('div#content')
  .append('<h2 style="text-align:center">tag: '+tag+'</h2>')
  .append('<div id="main-container"></div>');
  request({url:'/_view/tags?reduce=false&include_docs=true&key="'+tag+'"'}, function (e, resp) {
    resp.rows.forEach(function (row) {
      if (row.doc.description) {
        row.doc.htmlDescription = row.doc.description.split('&').join('&amp;')
                                             .split('"').join('&quot;')
                                             .split('<').join('&lt;')
                                             .split('>').join('&gt;')
      } else {
        row.doc.htmlDescription = ''
      }
      $('div#main-container').append(
        '<div class="all-package">' + 
          '<div class="tags-pkg-name"><a href="/#/'+encodeURIComponent(row.key)+'">' + row.id + '</a></div>' +
          '<div class="tags-pkg-desc">'+row.doc.htmlDescription+'</div>' +
        '</div>' +
        '<div class="spacer"></div>'
      );
      
    })
  })
}
app.author = function () {
  var author = this.params.author;
  clearContent();
  $('div#content')
  .append('<h2 style="text-align:center">author: '+escapeHTML(author)+'</h2>')
  .append('<div id="main-container"></div>');
  request({url:'/_view/author?reduce=false&include_docs=true&key="'+author+'"'}, function (e, resp) {
    resp.rows.forEach(function (row) {
      if (row.doc.description) {
        row.doc.htmlDescription = row.doc.description.split('&').join('&amp;')
                                             .split('"').join('&quot;')
                                             .split('<').join('&lt;')
                                             .split('>').join('&gt;')
      } else {
        row.doc.htmlDescription = ''
      }
      $('div#main-container').append(
        '<div class="all-package">' + 
          '<div class="tags-pkg-name"><a href="/#/'+encodeURIComponent(row.id)+'">' + row.id + '</a></div>' +
          '<div class="tags-pkg-desc">'+row.doc.htmlDescription+'</div>' +
        '</div>' +
        '<div class="spacer"></div>'
      );
      
    })
  })
}
app.analytics = function () {
  clearContent();
  var view = this.params.view || "thisweek";
  $('div#content').html(
    '<div id="browse-anchors">' +
      '<a href="/#/_analytics/thisweek">This Week</a>' +
      '<a href="/#/_analytics/30days">30 Days</a>' +
      '<a href="/#/_analytics/alltime">All Time</a>' +
    '</div>' +
    '<div id="main-container">' +
      '<div id="analytics-created"></div>' +
      '<div id="analytics-latest"></div>' +
      '<div id="analytics-updated"></div>' +
    '</div>'  
  )
  
   $('a:exactly("'+{thisweek:'This Week', '30days':'30 Days', alltime:'All Time'}[view]+'")').css('text-decoration', 'underline');
  
  $.getScript('/highcharts/highcharts.js', function () {
    var dt = new Date();
    if (view == 'thisweek') {
      dt.setDate(dt.getDate() - 7);
    }
    if (view == '30days') {
      dt.setDate(dt.getDate() - 30);
    }
    
    var series = [];
    
    var extract = function (resp, name) {
      var times = {};
      resp.rows.reverse();
      resp.rows.forEach(function (row) {
        if (view === 'thisweek') {
          var t = row.key[1].slice(0, 10);
        }
        if (view === '30days') {
          var t = row.key[1].slice(8, 10);
        }
        if (view === 'alltime') {
          var t = row.key[1].slice(0, 7);
        }
        
        if (!times[t]) times[t] = 0;
        times[t] += 1;
      })
      var x = {name:name, data:[]};
      for (i in times) {
        x.data.push(times[i]);
      }
      series.push(x);
      if (series.length === 3) graph(times);
    }
    
    graphNames = 
      { thisweek: 'This Week'
      , '30days': '30 Days' 
      , alltime: 'All Time'
      }
    
    var graph = function (times) {
      chart = new Highcharts.Chart(
        {
          chart: {
             renderTo: 'analytics-created',
             defaultSeriesType: 'line'
          },
          title: {
             text: graphNames[view]
          },
          xAxis: {
             categories: Object.keys(times)
          },
          tooltip: {
             enabled: false,
             formatter: function() {
                return '<b>'+ this.series.name +'</b><br/>'+
                   this.x +': '+ this.y +'Â°C';
             }
          },
          plotOptions: {
             line: {
                dataLabels: {
                   enabled: true
                },
                enableMouseTracking: false
             }
          },
          series: series
       });
    }
    
    if (view === 'thisweek' || view === '30days') {
      var endkey = dt.toISOString().slice(0, 10);
    }
    if (view === 'alltime') {
      var endkey = null;
    }
    
    request({url:'/_view/analytics?'+param(
      {reduce:'false'
      , descending:'true'
      , endkey:JSON.stringify(['created', endkey])
      , startkey:JSON.stringify(['created', new Date()])
      })}, 
      function (e, resp) {
        extract(resp, 'created')
    })
    request({url:'/_view/analytics?'+param(
      {reduce:'false'
      , descending:'true'
      , endkey:JSON.stringify(['update', endkey])
      , startkey:JSON.stringify(['update', new Date()])
      })}, 
      function (e, resp) {
        
        extract(resp, 'updated')
    })
    request({url:'/_view/analytics?'+param(
      {reduce:'false'
      , descending:'true'
      , endkey:JSON.stringify(['latest', endkey])
      , startkey:JSON.stringify(['latest', new Date()])
      })}, 
      function (e, resp) {
        extract(resp, 'latest')
    })
  })
}

$(function () { 
  app.s = $.sammy(function () {
    // Index of all databases
    this.get('', app.index);
    this.get("#/", app.index);
    this.get("#/_analytics", app.analytics);
    this.get("#/_analytics/:view", app.analytics);
    this.get("#/_browse", app.browse);
    this.get("#/_browse/:view", app.browse);
    this.get("#/_tag/:tag", app.tags);
    this.get("#/_author/:author", app.author);
    this.get("#/_install", function () {
      clearContent();
      request({url:'/install.html', dataType:'html'}, function (e, resp) {
        $('div#content').html('<div id="main-container">'+resp+'</div>');
      })
    });
    this.get("#/_publish", function () {
      clearContent();
      request({url:'/publish.html', dataType:'html'}, function (e, resp) {
        $('div#content').html('<div id="main-container">'+resp+'</div>');
      })
    });
    this.get("#/_more", function () {
      clearContent();
      request({url:'/more.html', dataType:'html'}, function (e, resp) {
        $('div#content').html('<div id="main-container">'+resp+'</div>');
      })
    });
    this.get("#/:id", app.showPackage);
    
    
  })
  app.s.run();
});
