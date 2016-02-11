/*jshint asi:true */

var debug           //= true;
var test            = debug  ? function () {} : require('tap').test
var test_           = !debug ? function () {} : require('tap').test
  , path            = require('path')
  , fs              = require('fs')
  , util            = require('util')
  , TransformStream = require('readable-stream').Transform
  , through         = require('through2')
  , streamapi       = require('../stream-api')
  , readdirp        = require('..')
  , root            = path.join(__dirname, 'bed')
  , totalDirs       = 6
  , totalFiles      = 12
  , ext1Files       = 4
  , ext2Files       = 3
  , ext3Files       = 2
  ;
  
// see test/readdirp.js for test bed layout

function opts (extend) {
  var o = { root: root };

  if (extend) {
    for (var prop in extend) {
      o[prop] = extend[prop];
    }
  }
  return o;
}

function capture () {
  var result = { entries: [], errors: [], ended: false }
    , dst = new TransformStream({ objectMode: true });

  dst._transform = function (entry, _, cb) {
    result.entries.push(entry);
    cb();
  }

  dst._flush = function (cb) {
    result.ended = true;
    this.push(result); 
    cb();
  }

  return dst;
}

test('\nintegrated', function (t) {
  t.test('\n# reading root without filter', function (t) {
    t.plan(2);

    readdirp(opts())
      .on('error', function (err) {
        t.fail('should not throw error', err);
      })
      .pipe(capture())
      .pipe(through.obj(
        function (result, _ , cb) { 
          t.equals(result.entries.length, totalFiles, 'emits all files');
          t.ok(result.ended, 'ends stream');
          t.end();
          cb();
        }
      ));
  })

  t.test('\n# normal: ["*.ext1", "*.ext3"]', function (t) {
    t.plan(2);

    readdirp(opts( { fileFilter: [ '*.ext1', '*.ext3' ] } ))
      .on('error', function (err) {
        t.fail('should not throw error', err);
      })
      .pipe(capture())
      .pipe(through.obj(
        function (result, _ , cb) { 
          t.equals(result.entries.length, ext1Files + ext3Files, 'all ext1 and ext3 files');
          t.ok(result.ended, 'ends stream');
          t.end();
          cb();
        }
      ))
  })

  t.test('\n# files only', function (t) {
    t.plan(2);

    readdirp(opts( { entryType: 'files' } ))
      .on('error', function (err) {
        t.fail('should not throw error', err);
      })
      .pipe(capture())
      .pipe(through.obj(
        function (result, _ , cb) { 
          t.equals(result.entries.length, totalFiles, 'returned files');
          t.ok(result.ended, 'ends stream');
          t.end();
          cb();
        }
      ))
  })

  t.test('\n# directories only', function (t) {
    t.plan(2);

    readdirp(opts( { entryType: 'directories' } ))
      .on('error', function (err) {
        t.fail('should not throw error', err);
      })
      .pipe(capture())
      .pipe(through.obj(
        function (result, _ , cb) { 
          t.equals(result.entries.length, totalDirs, 'returned directories');
          t.ok(result.ended, 'ends stream');
          t.end();
          cb();
        }
      ))
  })

  t.test('\n# both directories + files', function (t) {
    t.plan(2);

    readdirp(opts( { entryType: 'both' } ))
      .on('error', function (err) {
        t.fail('should not throw error', err);
      })
      .pipe(capture())
      .pipe(through.obj(
        function (result, _ , cb) { 
          t.equals(result.entries.length, totalDirs + totalFiles, 'returned everything');
          t.ok(result.ended, 'ends stream');
          t.end();
          cb();
        }
      ))
  })

  t.test('\n# directory filter with directories only', function (t) {
    t.plan(2);

    readdirp(opts( { entryType: 'directories', directoryFilter: [ 'root_dir1', '*dir1_subdir1' ] } ))
      .on('error', function (err) {
        t.fail('should not throw error', err);
      })
      .pipe(capture())
      .pipe(through.obj(
        function (result, _ , cb) {
          t.equals(result.entries.length, 2, 'two directories');
          t.ok(result.ended, 'ends stream');
          t.end();
          cb();
        }
      ))
  })

  t.test('\n# directory and file filters with both entries', function (t) {
    t.plan(2);

    readdirp(opts( { entryType: 'both', directoryFilter: [ 'root_dir1', '*dir1_subdir1' ], fileFilter: [ '!*.ext1' ] } ))
      .on('error', function (err) {
        t.fail('should not throw error', err);
      })
      .pipe(capture())
      .pipe(through.obj(
        function (result, _ , cb) {
          t.equals(result.entries.length, 6, '2 directories and 4 files');
          t.ok(result.ended, 'ends stream');
          t.end();
          cb();
        }
      ))
  })

  t.test('\n# negated: ["!*.ext1", "!*.ext3"]', function (t) {
    t.plan(2);

    readdirp(opts( { fileFilter: [ '!*.ext1', '!*.ext3' ] } ))
      .on('error', function (err) {
        t.fail('should not throw error', err);
      })
      .pipe(capture())
      .pipe(through.obj(
        function (result, _ , cb) { 
          t.equals(result.entries.length, totalFiles - ext1Files - ext3Files, 'all but ext1 and ext3 files');
          t.ok(result.ended, 'ends stream');
          t.end();
        }
      ))
  })

  t.test('\n# no options given', function (t) {
    t.plan(1);
    readdirp()
      .on('error', function (err) {
        t.similar(err.toString() , /Need to pass at least one argument/ , 'emits meaningful error');
        t.end();
      })
  })

  t.test('\n# mixed: ["*.ext1", "!*.ext3"]', function (t) {
    t.plan(1);

    readdirp(opts( { fileFilter: [ '*.ext1', '!*.ext3' ] } ))
      .on('error', function (err) {
        t.similar(err.toString() , /Cannot mix negated with non negated glob filters/ , 'emits meaningful error');
        t.end();
      })
  })
})


test('\napi separately', function (t) {


  t.test('\n# handleError', function (t) {
    t.plan(1);

    var api = streamapi()
      , warning = new Error('some file caused problems');

    api.stream
      .on('warn', function (err) {
        t.equals(err, warning, 'warns with the handled error');
      })
    api.handleError(warning);
  })

  t.test('\n# when stream is paused and then resumed', function (t) {
    t.plan(6);
    var api = streamapi()
      , resumed = false
      , fatalError = new Error('fatal!')
      , nonfatalError = new Error('nonfatal!')
      , processedData = 'some data'
      ;

    api.stream
      .on('warn', function (err) {
        t.equals(err, nonfatalError, 'emits the buffered warning');
        t.ok(resumed, 'emits warning only after it was resumed');
      })
      .on('error', function (err) {
        t.equals(err, fatalError, 'emits the buffered fatal error');
        t.ok(resumed, 'emits errors only after it was resumed');
      })
      .on('data', function (data) {
        t.equals(data, processedData, 'emits the buffered data');
        t.ok(resumed, 'emits data only after it was resumed');
      })
      .pause()
    
    api.processEntry(processedData);
    api.handleError(nonfatalError);
    api.handleFatalError(fatalError);
  
    setTimeout(function () {
      resumed = true;
      api.stream.resume();
    }, 1)
  })


  t.test('\n# when a stream is destroyed, it emits "closed", but no longer emits "data", "warn" and "error"', function (t) {
    var api = streamapi()
      , fatalError = new Error('fatal!')
      , nonfatalError = new Error('nonfatal!')
      , processedData = 'some data'
      , plan = 0;

    t.plan(6)
    var stream = api.stream
      .on('warn', function (err) {
        t.ok(!stream._destroyed, 'emits warning until destroyed');
      })
      .on('error', function (err) {
        t.ok(!stream._destroyed, 'emits errors until destroyed');
      })
      .on('data', function (data) {
        t.ok(!stream._destroyed, 'emits data until destroyed');
      })
      .on('close', function () {
        t.ok(stream._destroyed, 'emits close when stream is destroyed');
      })
    

    api.processEntry(processedData);
    api.handleError(nonfatalError);
    api.handleFatalError(fatalError);

    setTimeout(function () {
      stream.destroy()

      t.notOk(stream.readable, 'stream is no longer readable after it is destroyed')

      api.processEntry(processedData);
      api.handleError(nonfatalError);
      api.handleFatalError(fatalError);

      process.nextTick(function () {
        t.pass('emits no more data, warn or error events after it was destroyed')  
        t.end();
      })
    }, 10)
  })
})
