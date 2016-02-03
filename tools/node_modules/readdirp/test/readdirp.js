/*jshint asi:true */

var test     = require('tap').test
  , path     = require('path')
  , fs       = require('fs')
  , util     = require('util')
  , net      = require('net')
  , readdirp = require('../readdirp.js')
  , root     = path.join(__dirname, '../test/bed')
  , totalDirs          =  6
  , totalFiles         =  12
  , ext1Files          =  4
  , ext2Files          =  3
  , ext3Files          =  2
  , rootDir2Files      =  2
  , nameHasLength9Dirs =  2
  , depth1Files        =  8
  , depth0Files        =  3
  ;

/*
Structure of test bed:
    .
    ├── root_dir1
    │   ├── root_dir1_file1.ext1
    │   ├── root_dir1_file2.ext2
    │   ├── root_dir1_file3.ext3
    │   ├── root_dir1_subdir1
    │   │   └── root1_dir1_subdir1_file1.ext1
    │   └── root_dir1_subdir2
    │       └── .gitignore
    ├── root_dir2
    │   ├── root_dir2_file1.ext1
    │   ├── root_dir2_file2.ext2
    │   ├── root_dir2_subdir1
    │   │   └── .gitignore
    │   └── root_dir2_subdir2
    │       └── .gitignore
    ├── root_file1.ext1
    ├── root_file2.ext2
    └── root_file3.ext3

    6 directories, 13 files
*/

// console.log('\033[2J'); // clear console

function opts (extend) {
  var o = { root: root };

  if (extend) {
    for (var prop in extend) {
      o[prop] = extend[prop];
    }
  }
  return o;
}

test('\nreading root without filter', function (t) {
  t.plan(2);
  readdirp(opts(), function (err, res) {
    t.equals(res.directories.length, totalDirs, 'all directories');
    t.equals(res.files.length, totalFiles, 'all files');
    t.end();
  })
})

test('\nreading root without filter using lstat', function (t) {
  t.plan(2);
  readdirp(opts({ lstat: true }), function (err, res) {
    t.equals(res.directories.length, totalDirs, 'all directories');
    t.equals(res.files.length, totalFiles, 'all files');
    t.end();
  })
})

test('\nreading root with symlinks using lstat', function (t) {
  t.plan(2);
  fs.symlinkSync(path.join(root, 'root_dir1'), path.join(root, 'dirlink'));
  fs.symlinkSync(path.join(root, 'root_file1.ext1'), path.join(root, 'link.ext1'));
  readdirp(opts({ lstat: true }), function (err, res) {
    t.equals(res.directories.length, totalDirs, 'all directories');
    t.equals(res.files.length, totalFiles + 2, 'all files + symlinks');
    fs.unlinkSync(path.join(root, 'dirlink'));
    fs.unlinkSync(path.join(root, 'link.ext1'));
    t.end();
  })
})

test('\nreading non-standard fds', function (t) {
  t.plan(2);
  var server = net.createServer().listen(path.join(root, 'test.sock'), function(){
    readdirp(opts({ entryType: 'all' }), function (err, res) {
      t.equals(res.files.length, totalFiles + 1, 'all files + socket');
      readdirp(opts({ entryType: 'both' }), function (err, res) {
        t.equals(res.files.length, totalFiles, 'all regular files only');
        server.close();
        t.end();
      })
    })
  });
})

test('\nreading root using glob filter', function (t) {
  // normal
  t.test('\n# "*.ext1"', function (t) {
    t.plan(1);
    readdirp(opts( { fileFilter: '*.ext1' } ), function (err, res) {
      t.equals(res.files.length, ext1Files, 'all ext1 files');
      t.end();
    })
  })
  t.test('\n# ["*.ext1", "*.ext3"]', function (t) {
    t.plan(1);
    readdirp(opts( { fileFilter: [ '*.ext1', '*.ext3' ] } ), function (err, res) {
      t.equals(res.files.length, ext1Files + ext3Files, 'all ext1 and ext3 files');
      t.end();
    })
  })
  t.test('\n# "root_dir1"', function (t) {
    t.plan(1);
    readdirp(opts( { directoryFilter: 'root_dir1' }), function (err, res) {
      t.equals(res.directories.length, 1, 'one directory');
      t.end();
    })
  })
  t.test('\n# ["root_dir1", "*dir1_subdir1"]', function (t) {
    t.plan(1);
    readdirp(opts( { directoryFilter: [ 'root_dir1', '*dir1_subdir1' ]}), function (err, res) {
      t.equals(res.directories.length, 2, 'two directories');
      t.end();
    })
  })

  t.test('\n# negated: "!*.ext1"', function (t) {
    t.plan(1);
    readdirp(opts( { fileFilter: '!*.ext1' } ), function (err, res) {
      t.equals(res.files.length, totalFiles - ext1Files, 'all but ext1 files');
      t.end();
    })
  })
  t.test('\n# negated: ["!*.ext1", "!*.ext3"]', function (t) {
    t.plan(1);
    readdirp(opts( { fileFilter: [ '!*.ext1', '!*.ext3' ] } ), function (err, res) {
      t.equals(res.files.length, totalFiles - ext1Files - ext3Files, 'all but ext1 and ext3 files');
      t.end();
    })
  })

  t.test('\n# mixed: ["*.ext1", "!*.ext3"]', function (t) {
    t.plan(1);
    readdirp(opts( { fileFilter: [ '*.ext1', '!*.ext3' ] } ), function (err, res) {
      t.similar(err[0].toString(), /Cannot mix negated with non negated glob filters/, 'returns meaningfull error');
      t.end();
    })
  })

  t.test('\n# leading and trailing spaces: [" *.ext1", "*.ext3 "]', function (t) {
    t.plan(1);
    readdirp(opts( { fileFilter: [ ' *.ext1', '*.ext3 ' ] } ), function (err, res) {
      t.equals(res.files.length, ext1Files + ext3Files, 'all ext1 and ext3 files');
      t.end();
    })
  })
  t.test('\n# leading and trailing spaces: [" !*.ext1", " !*.ext3 "]', function (t) {
    t.plan(1);
    readdirp(opts( { fileFilter: [ ' !*.ext1', ' !*.ext3' ] } ), function (err, res) {
      t.equals(res.files.length, totalFiles - ext1Files - ext3Files, 'all but ext1 and ext3 files');
      t.end();
    })
  })

  t.test('\n# ** glob pattern', function (t) {
    t.plan(1);
    readdirp(opts( { fileFilter: '**/*.ext1' } ), function (err, res) {
      t.equals(res.files.length, ext1Files, 'ignores ** in **/*.ext1 -> only *.ext1 files');
      t.end();
    })
  })
})

test('\n\nreading root using function filter', function (t) {
  t.test('\n# file filter -> "contains root_dir2"', function (t) {
    t.plan(1);
    readdirp(
        opts( { fileFilter: function (fi) { return fi.name.indexOf('root_dir2') >= 0; } })
      , function (err, res) {
          t.equals(res.files.length, rootDir2Files, 'all rootDir2Files');
          t.end();
      }
    )
  })

  t.test('\n# directory filter -> "name has length 9"', function (t) {
    t.plan(1);
    readdirp(
        opts( { directoryFilter: function (di) { return di.name.length === 9; } })
      , function (err, res) {
          t.equals(res.directories.length, nameHasLength9Dirs, 'all all dirs with name length 9');
          t.end();
      }
    )
  })
})

test('\nreading root specifying maximum depth', function (t) {
  t.test('\n# depth 1', function (t) {
    t.plan(1);
      readdirp(opts( { depth: 1 } ), function (err, res) {
        t.equals(res.files.length, depth1Files, 'does not return files at depth 2');
      })
  })
})

test('\nreading root with no recursion', function (t) {
  t.test('\n# depth 0', function (t) {
    t.plan(1);
      readdirp(opts( { depth: 0 } ), function (err, res) {
        t.equals(res.files.length, depth0Files, 'does not return files at depth 0');
      })
  })
})

test('\nprogress callbacks', function (t) {
  t.plan(2);

  var pluckName = function(fi) { return fi.name; }
    , processedFiles = [];

  readdirp(
      opts()
    , function(fi) {
        processedFiles.push(fi);
      }
    , function (err, res) {
        t.equals(processedFiles.length, res.files.length, 'calls back for each file processed');
        t.deepEquals(processedFiles.map(pluckName).sort(),res.files.map(pluckName).sort(), 'same file names');
        t.end();
      }
  )
})

test('resolving of name, full and relative paths', function (t) {
  var expected = {
        name          :  'root_dir1_file1.ext1'
      , parentDirName :  'root_dir1'
      , path          :  'root_dir1/root_dir1_file1.ext1'
      , fullPath      :  'test/bed/root_dir1/root_dir1_file1.ext1'
      }
    , opts = [
        { root: './bed'          ,  prefix: ''     }
      , { root: './bed/'         ,  prefix: ''     }
      , { root: 'bed'            ,  prefix: ''     }
      , { root: 'bed/'           ,  prefix: ''     }
      , { root: '../test/bed/'   ,  prefix: ''     }
      , { root: '.'              ,  prefix: 'bed'  }
    ]
  t.plan(opts.length);

  opts.forEach(function (op) {
    op.fileFilter = 'root_dir1_file1.ext1';

    t.test('\n' + util.inspect(op), function (t) {
      t.plan(4);

      readdirp (op, function(err, res) {
        t.equals(res.files[0].name, expected.name, 'correct name');
        t.equals(res.files[0].path, path.join(op.prefix, expected.path), 'correct path');
      })

      fs.realpath(op.root, function(err, fullRoot) {
        readdirp (op, function(err, res) {
          t.equals(
              res.files[0].fullParentDir
            , path.join(fullRoot, op.prefix, expected.parentDirName)
            , 'correct parentDir'
          );
          t.equals(
              res.files[0].fullPath
            , path.join(fullRoot, op.prefix, expected.parentDirName, expected.name)
            , 'correct fullPath'
          );
        })
      })
    })
  })
})


