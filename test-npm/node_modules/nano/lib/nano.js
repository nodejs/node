// Licensed under the Apache License, Version 2.0 (the 'License'); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an 'AS IS' BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

'use strict';

var u = require('url');
var assert = require('assert');
var querystring = require('querystring');
var request = require('request');
var errs = require('errs');
var _ = require('underscore');
var follow = require('follow');
var logger = require('./logger');

var nano;

module.exports = exports = nano = function dbScope(cfg) {
  var serverScope = {};

  if (typeof cfg === 'string') {
    cfg = {url: cfg};
  }

  assert.equal(typeof cfg, 'object',
    'You must specify the endpoint url when invoking this module');
  assert.ok(/^https?:/.test(cfg.url), 'url is not valid');

  cfg = _.clone(cfg);

  serverScope.config = cfg;
  cfg.requestDefaults = cfg.requestDefaults || {jar: false};

  var httpAgent = (typeof cfg.request === 'function') ? cfg.request :
    request.defaults(cfg.requestDefaults);
  var followAgent = (typeof cfg.follow === 'function') ? cfg.follow : follow;
  var log = typeof cfg.log === 'function' ? cfg.log : logger(cfg);
  var parseUrl = 'parseUrl' in cfg ? cfg.parseUrl : true;

  function maybeExtractDatabaseComponent() {
    if (!parseUrl) {
      return;
    }

    var path = u.parse(cfg.url);
    var pathArray = path.pathname.split('/').filter(function(e) { return e; });
    var db = pathArray.pop();
    var rootPath = path.pathname.replace(/\/?$/, '/..');

    if (db) {
      cfg.url = urlResolveFix(cfg.url, rootPath).replace(/\/?$/, '');
      return db;
    }
  }
  
  function scrub(str) {
    if (str) {
      str = str.replace(/\/\/(.*)@/,"//XXXXXX:XXXXXX@");
    }
    return str;
  }

  function relax(opts, callback) {
    if (typeof opts === 'function') {
      callback = opts;
      opts = {path: ''};
    }

    if (typeof opts === 'string') {
      opts = {path: opts};
    }

    if (!opts) {
      opts = {path: ''};
      callback = null;
    }

    var qs = _.extend({}, opts.qs);

    var headers = {
      'content-type': 'application/json',
      accept: 'application/json'
    };

    var req = {
      method: (opts.method || 'GET'),
      headers: headers,
      uri: cfg.url
    };

    var parsed;
    var rh;

    // https://github.com/mikeal/request#requestjar
    var isJar = opts.jar || cfg.jar;

    if (isJar) {
      req.jar = isJar;
    }

    // http://wiki.apache.org/couchdb/HTTP_database_API#Naming_and_Addressing
    if (opts.db) {
      req.uri = urlResolveFix(req.uri, encodeURIComponent(opts.db));
    }

    if (opts.multipart) {
      req.multipart = opts.multipart;
    }

    req.headers = _.extend(req.headers, opts.headers, cfg.defaultHeaders);

    if (opts.path) {
      req.uri += '/' + opts.path;
    } else if (opts.doc) {
      if (!/^_design/.test(opts.doc)) {
        // http://wiki.apache.org/couchdb/HTTP_Document_API#Naming.2FAddressing
        req.uri += '/' + encodeURIComponent(opts.doc);
      } else {
        // http://wiki.apache.org/couchdb/HTTP_Document_API#Document_IDs
        req.uri += '/' + opts.doc;
      }

      // http://wiki.apache.org/couchdb/HTTP_Document_API#Attachments
      if (opts.att) {
        req.uri += '/' + opts.att;
      }
    }

    // prevent bugs where people set encoding when piping
    if (opts.encoding !== undefined && callback) {
      req.encoding = opts.encoding;
      delete req.headers['content-type'];
      delete req.headers.accept;
    }

    if (opts.contentType) {
      req.headers['content-type'] = opts.contentType;
      delete req.headers.accept;
    }

    // http://guide.couchdb.org/draft/security.html#cookies
    if (cfg.cookie) {
      req.headers['X-CouchDB-WWW-Authenticate'] = 'Cookie';
      req.headers.cookie = cfg.cookie;
    }

    // http://wiki.apache.org/couchdb/HTTP_view_API#Querying_Options
    if (typeof opts.qs === 'object' && !_.isEmpty(opts.qs)) {
      ['startkey', 'endkey', 'key', 'keys'].forEach(function(key) {
        if (key in opts.qs) {
          qs[key] = JSON.stringify(opts.qs[key]);
        }
      });
      req.qs = qs;
    }

    if (opts.body) {
      if (Buffer.isBuffer(opts.body) || opts.dontStringify) {
        req.body = opts.body;
      } else {
        req.body = JSON.stringify(opts.body, function(key, value) {
          // don't encode functions
          if (typeof(value) === 'function') {
            return value.toString();
          } else {
            return value;
          }
        });
      }
    }

    if (opts.form) {
      req.headers['content-type'] =
        'application/x-www-form-urlencoded; charset=utf-8';
      req.body = querystring.stringify(opts.form).toString('utf8');
    }

    log(req);

    if (!callback) {
      return httpAgent(req);
    }

    return httpAgent(req, function(e, h, b) {
      rh = h && h.headers || {};
      rh.statusCode = h && h.statusCode || 500;
      rh.uri = req.uri;

      if (e) {
        log({err: 'socket', body: b, headers: rh});
        return callback(errs.merge(e, {
          message: 'error happened in your connection',
          scope: 'socket',
          errid: 'request'
        }));
      }

      delete rh.server;
      delete rh['content-length'];

      if (opts.dontParse) {
        parsed = b;
      } else {
        try { parsed = JSON.parse(b); } catch (err) { parsed = b; }
      }

      if (rh.statusCode >= 200 && rh.statusCode < 400) {
        log({err: null, body: parsed, headers: rh});
        return callback(null, parsed, rh);
      }

      log({err: 'couch', body: parsed, headers: rh});

      // cloudant stacktrace
      if (typeof parsed === 'string') {
        parsed = {message: parsed};
      }

      if (!parsed.message && (parsed.reason || parsed.error)) {
        parsed.message = (parsed.reason || parsed.error);
      }

      // fix cloudant issues where they give an erlang stacktrace as js
      delete parsed.stack;

      // scrub credentials
      req.uri = scrub(req.uri);
      rh.uri = scrub(rh.uri);
      if (req.headers.cookie) {
        req.headers.cookie = "XXXXXXX";
      }

      callback(errs.merge({
        message: 'couch returned ' + rh.statusCode,
        scope: 'couch',
        statusCode: rh.statusCode,
        request: req,
        headers: rh,
        errid: 'non_200'
      }, errs.create(parsed)));
    });
  }

  // http://docs.couchdb.org/en/latest/api/server/authn.html#cookie-authentication
  function auth(username, password, callback) {
    return relax({
      method: 'POST',
      db: '_session',
      form: {
        name: username,
        password: password
      }
    }, callback);
  }

  // http://docs.couchdb.org/en/latest/api/server/authn.html#post--_session
  function session(callback) {
    return relax({db: '_session'}, callback);
  }

  // http://docs.couchdb.org/en/latest/api/server/common.html#get--_db_updates
  function updates(qs, callback) {
    if (typeof qs === 'function') {
      callback = qs;
      qs = {};
    }
    return relax({
      db: '_db_updates',
      qs: qs
    }, callback);
  }

  function followUpdates(qs, callback) {
    return followDb('_db_updates', qs, callback);
  }

  // http://docs.couchdb.org/en/latest/api/database/common.html#put--db
  function createDb(dbName, callback) {
    return relax({db: dbName, method: 'PUT'}, callback);
  }

  // http://docs.couchdb.org/en/latest/api/database/common.html#delete--db
  function destroyDb(dbName, callback) {
    return relax({db: dbName, method: 'DELETE'}, callback);
  }

  // http://docs.couchdb.org/en/latest/api/database/common.html#get--db
  function getDb(dbName, callback) {
    return relax({db: dbName}, callback);
  }

  // http://docs.couchdb.org/en/latest/api/server/common.html#get--_all_dbs
  function listDbs(callback) {
    return relax({db: '_all_dbs'}, callback);
  }

  // http://docs.couchdb.org/en/latest/api/database/compact.html#post--db-_compact
  function compactDb(dbName, ddoc, callback) {
    if (typeof ddoc === 'function') {
      callback = ddoc;
      ddoc = null;
    }
    return relax({
      db: dbName,
      doc: '_compact',
      att: ddoc,
      method: 'POST'
    }, callback);
  }

  // http://docs.couchdb.org/en/latest/api/database/changes.html#get--db-_changes
  function changesDb(dbName, qs, callback) {
    if (typeof qs === 'function') {
      callback = qs;
      qs = {};
    }
    return relax({db: dbName, path: '_changes', qs: qs}, callback);
  }

  function followDb(dbName, qs, callback) {
    if (typeof qs === 'function') {
      callback = qs;
      qs = {};
    }

    qs.db = urlResolveFix(cfg.url, encodeURIComponent(dbName));

    if (typeof callback === 'function') {
      return followAgent(qs, callback);
    } else {
      return new followAgent.Feed(qs);
    }
  }

  function _serializeAsUrl(db) {
    if (typeof db === 'object' && db.config && db.config.url && db.config.db) {
      return urlResolveFix(db.config.url, encodeURIComponent(db.config.db));
    } else {
      return db;
    }
  }

  // http://docs.couchdb.org/en/latest/api/server/common.html#post--_replicate
  function replicateDb(source, target, opts, callback) {
    if (typeof opts === 'function') {
      callback = opts;
      opts = {};
    }

    opts.source = _serializeAsUrl(source);
    opts.target = _serializeAsUrl(target);

    return relax({db: '_replicate', body: opts, method: 'POST'}, callback);
  }

  function docModule(dbName) {
    var docScope = {};
    dbName = decodeURIComponent(dbName);

    // http://docs.couchdb.org/en/latest/api/document/common.html#put--db-docid
    // http://docs.couchdb.org/en/latest/api/database/common.html#post--db
    function insertDoc(doc, qs, callback) {
      var opts = {db: dbName, body: doc, method: 'POST'};

      if (typeof qs === 'function') {
        callback = qs;
        qs = {};
      }

      if (typeof qs === 'string') {
        qs = {docName: qs};
      }

      if (qs) {
        if (qs.docName) {
          opts.doc = qs.docName;
          opts.method = 'PUT';
          delete qs.docName;
        }
        opts.qs = qs;
      }

      return relax(opts, callback);
    }

    // http://docs.couchdb.org/en/latest/api/document/common.html#delete--db-docid
    function destroyDoc(docName, rev, callback) {
      return relax({
        db: dbName,
        doc: docName,
        method: 'DELETE',
        qs: {rev: rev}
      }, callback);
    }

    // http://docs.couchdb.org/en/latest/api/document/common.html#get--db-docid
    function getDoc(docName, qs, callback) {
      if (typeof qs === 'function') {
        callback = qs;
        qs = {};
      }

      return relax({db: dbName, doc: docName, qs: qs}, callback);
    }

    // http://docs.couchdb.org/en/latest/api/document/common.html#head--db-docid
    function headDoc(docName, callback) {
      return relax({
        db: dbName,
        doc: docName,
        method: 'HEAD',
        qs: {}
      }, callback);
    }

    // http://docs.couchdb.org/en/latest/api/document/common.html#copy--db-docid
    function copyDoc(docSrc, docDest, opts, callback) {
      if (typeof opts === 'function') {
        callback = opts;
        opts = {};
      }

      var qs = {
        db: dbName,
        doc: docSrc,
        method: 'COPY',
        headers: {'Destination': docDest}
      };

      if (opts.overwrite) {
        return headDoc(docDest, function(e, b, h) {
          if (e && e.statusCode !== 404) {
            return callback(e);
          }
          qs.headers.Destination += '?rev=' +
            h.etag.substring(1, h.etag.length - 1);
          return relax(qs, callback);
        });
      } else {
        return relax(qs, callback);
      }
    }

    // http://docs.couchdb.org/en/latest/api/database/bulk-api.html#get--db-_all_docs
    function listDoc(qs, callback) {
      if (typeof qs === 'function') {
        callback = qs;
        qs = {};
      }

      return relax({db: dbName, path: '_all_docs', qs: qs}, callback);
    }

    // http://docs.couchdb.org/en/latest/api/database/bulk-api.html#post--db-_all_docs
    function fetchDocs(docNames, qs, callback) {
      if (typeof qs === 'function') {
        callback = qs;
        qs = {};
      }

      qs['include_docs'] = true;

      return relax({
        db: dbName,
        path: '_all_docs',
        method: 'POST',
        qs: qs,
        body: docNames
      }, callback);
    }

    function fetchRevs(docNames, qs, callback) {
      if (typeof qs === 'function') {
        callback = qs;
        qs = {};
      }
      return relax({
        db: dbName,
        path: '_all_docs',
        method: 'POST',
        qs: qs,
        body: docNames
      }, callback);
    }

    function view(ddoc, viewName, meta, qs, callback) {
      if (typeof qs === 'function') {
        callback = qs;
        qs = {};
      }

      var viewPath = '_design/' + ddoc + '/_' + meta.type + '/'  + viewName;

      // Several search parameters must be JSON-encoded; but since this is an
      // object API, several parameters need JSON endoding.
      var paramsToEncode = ['counts', 'drilldown', 'group_sort', 'ranges', 'sort'];
      paramsToEncode.forEach(function(param) {
        if (param in qs) {
          qs[param] = JSON.stringify(qs[param]);
        }
      });

      if (qs && qs.keys) {
        var body = {keys: qs.keys};
        delete qs.keys;
        return relax({
          db: dbName,
          path: viewPath,
          method: 'POST',
          qs: qs,
          body: body
        }, callback);
      } else {
        var req = {
          db: dbName,
          method: meta.method || 'GET',
          path: viewPath,
          qs: qs
        };

        if (meta.body) {
          req.body = meta.body;
        }

        return relax(req, callback);
      }
    }

    // http://docs.couchdb.org/en/latest/api/ddoc/views.html#post--db-_design-ddoc-_view-view
    function viewDocs(ddoc, viewName, qs, callback) {
      return view(ddoc, viewName, {type: 'view'}, qs, callback);
    }

    // geocouch
    function viewSpatial(ddoc, viewName, qs, callback) {
      return view(ddoc, viewName, {type: 'spatial'}, qs, callback);
    }

    // cloudant
    function viewSearch(ddoc, viewName, qs, callback) {
      return view(ddoc, viewName, {type: 'search'}, qs, callback);
    }

    // http://docs.couchdb.org/en/latest/api/ddoc/render.html#get--db-_design-ddoc-_show-func
    function showDoc(ddoc, viewName, docName, qs, callback) {
      return view(ddoc, viewName + '/' + docName, {type: 'show'}, qs, callback);
    }

    // http://docs.couchdb.org/en/latest/api/ddoc/render.html#put--db-_design-ddoc-_update-func-docid
    function updateWithHandler(ddoc, viewName, docName, body, callback) {
      return view(ddoc, viewName + '/' + encodeURIComponent(docName), {
        type: 'update',
        method: 'PUT',
        body: body
      }, callback);
    }

    function viewWithList(ddoc, viewName, listName, qs, callback) {
      return view(ddoc, listName + '/' + viewName, {
        type: 'list'
      }, qs, callback);
    }

    // http://docs.couchdb.org/en/latest/api/database/bulk-api.html#post--db-_bulksDoc
    function bulksDoc(docs, qs, callback) {
      if (typeof qs === 'function') {
        callback = qs;
        qs = {};
      }

      return relax({
        db: dbName,
        path: '_bulk_docs',
        body: docs,
        method: 'POST',
        qs: qs
      }, callback);
    }

    // http://docs.couchdb.org/en/latest/api/document/common.html#creating-multiple-attachments
    function insertMultipart(doc, attachments, qs, callback) {
      if (typeof qs === 'string') {
        qs = {docName: qs};
      }

      var docName = qs.docName;
      delete qs.docName;

      doc = _.extend({_attachments: {}}, doc);

      var multipart = [];

      attachments.forEach(function(att) {
        doc._attachments[att.name] = {
          follows: true,
          length: Buffer.isBuffer(att.data) ?
            att.data.length : Buffer.byteLength(att.data),
          /* jscs:disable requireCamelCaseOrUpperCaseIdentifiers */
          'content_type': att.content_type
        };
        multipart.push({body: att.data});
      });

      multipart.unshift({
        'content-type': 'application/json',
        body: JSON.stringify(doc)
      });

      return relax({
        db: dbName,
        method: 'PUT',
        contentType: 'multipart/related',
        doc: docName,
        qs: qs,
        multipart: multipart
      }, callback);
    }

    function getMultipart(docName, qs, callback) {
      if (typeof qs === 'function') {
        callback = qs;
        qs = {};
      }

      qs.attachments = true;

      return relax({
        db: dbName,
        doc: docName,
        encoding: null,
        contentType: 'multipart/related',
        qs: qs
      }, callback);
    }

    function insertAtt(docName, attName, att, contentType, qs, callback) {
      if (typeof qs === 'function') {
        callback = qs;
        qs = {};
      }

      return relax({
        db: dbName,
        att: attName,
        method: 'PUT',
        contentType: contentType,
        doc: docName,
        qs: qs,
        body: att,
        dontStringify: true
      }, callback);
    }

    function getAtt(docName, attName, qs, callback) {
      if (typeof qs === 'function') {
        callback = qs;
        qs = {};
      }

      return relax({
        db: dbName,
        att: attName,
        doc: docName,
        qs: qs,
        encoding: null,
        dontParse: true
      }, callback);
    }

    function destroyAtt(docName, attName, qs, callback) {
      return relax({
        db: dbName,
        att: attName,
        method: 'DELETE',
        doc: docName,
        qs: qs
      }, callback);
    }

    // db level exports
    docScope = {
      info: function(cb) {
        return getDb(dbName, cb);
      },
      replicate: function(target, opts, cb) {
        return replicateDb(dbName, target, opts, cb);
      },
      compact: function(cb) {
        return compactDb(dbName, cb);
      },
      changes: function(qs, cb) {
        return changesDb(dbName, qs, cb);
      },
      follow: function(qs, cb) {
        return followDb(dbName, qs, cb);
      },
      auth: auth,
      session: session,
      insert: insertDoc,
      get: getDoc,
      head: headDoc,
      copy: copyDoc,
      destroy: destroyDoc,
      bulk: bulksDoc,
      list: listDoc,
      fetch: fetchDocs,
      fetchRevs: fetchRevs,
      config: {url: cfg.url, db: dbName},
      multipart: {
        insert: insertMultipart,
        get: getMultipart
      },
      attachment: {
        insert: insertAtt,
        get: getAtt,
        destroy: destroyAtt
      },
      show: showDoc,
      atomic: updateWithHandler,
      updateWithHandler: updateWithHandler,
      search: viewSearch,
      spatial: viewSpatial,
      view: viewDocs,
      viewWithList: viewWithList
    };

    docScope.view.compact = function(ddoc, cb) {
      return compactDb(dbName, ddoc, cb);
    };

    return docScope;
  }

  // server level exports
  serverScope = _.extend(serverScope, {
    db: {
      create: createDb,
      get: getDb,
      destroy: destroyDb,
      list: listDbs,
      use: docModule,
      scope: docModule,
      compact: compactDb,
      replicate: replicateDb,
      changes: changesDb,
      follow: followDb,
      followUpdates: followUpdates,
      updates: updates
    },
    use: docModule,
    scope: docModule,
    request: relax,
    relax: relax,
    dinosaur: relax,
    auth: auth,
    session: session,
    updates: updates,
    followUpdates: followUpdates
  });

  var db = maybeExtractDatabaseComponent();

  return db ? docModule(db) : serverScope;
};

/*
 * and now an ascii dinosaur
 *              _
 *            / _) ROAR! i'm a vegan!
 *     .-^^^-/ /
 *  __/       /
 * /__.|_|-|_|
 *
 * thanks for visiting! come again!
 */
nano.version = require('../package.json').version;
nano.path    = __dirname;

function urlResolveFix(couchUrl, dbName) {
  if (/[^\/]$/.test(couchUrl)) {
    couchUrl += '/';
  }
  return u.resolve(couchUrl, dbName);
}
