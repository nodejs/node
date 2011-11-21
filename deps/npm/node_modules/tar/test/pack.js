var tap = require("tap")
  , tar = require("../tar.js")
  , pkg = require("../package.json")
  , Pack = tar.Pack
  , fstream = require("fstream")
  , Reader = fstream.Reader
  , Writer = fstream.Writer
  , path = require("path")
  , input = path.resolve(__dirname, "fixtures/")
  , target = path.resolve(__dirname, "tmp/pack.tar")
  , uid = process.getuid ? process.getuid() : 0
  , gid = process.getgid ? process.getgid() : 0

  , entries =

    // the global header and root fixtures/ dir are going to get
    // a different date each time, so omit that bit.
    // Also, dev/ino values differ across machines, so that's not
    // included.  Rather than use 
    [ [ 'globalExtendedHeader',
      { path: 'PaxHeader/',
        mode: 438,
        uid: 0,
        gid: 0,
        type: 'g',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' },
      { "NODETAR.author": pkg.author,
        "NODETAR.name": pkg.name,
        "NODETAR.description": pkg.description,
        "NODETAR.version": pkg.version,
        "NODETAR.repository.type": pkg.repository.type,
        "NODETAR.repository.url": pkg.repository.url,
        "NODETAR.main": pkg.main,
        "NODETAR.scripts.test": pkg.scripts.test,
        "NODETAR.engines.node": pkg.engines.node } ]

    , [ 'entry',
      { path: 'fixtures/',
        mode: 493,
        uid: uid,
        gid: gid,
        size: 0,
        type: '5',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'extendedHeader',
      { path: 'PaxHeader/fixtures/200cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc',
        mode: 420,
        uid: uid,
        gid: gid,
        size: 402,
        mtime: new Date('Thu, 27 Oct 2011 03:41:08 GMT'),
        cksum: 13492,
        type: 'x',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' },
      { path: 'fixtures/200ccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc',
        'NODETAR.depth': '1',
        'NODETAR.type': 'File',
        nlink: 1,
        uid: uid,
        gid: gid,
        size: 200,
        'NODETAR.blksize': '4096',
        'NODETAR.blocks': '8' } ]

    , [ 'entry',
      { path: 'fixtures/200ccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc',
        mode: 420,
        uid: uid,
        gid: gid,
        size: 200,
        mtime: new Date('Thu, 27 Oct 2011 03:41:08 GMT'),
        cksum: 13475,
        type: '0',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '',
        'NODETAR.depth': '1',
        'NODETAR.type': 'File',
        nlink: 1,
        'NODETAR.blksize': '4096',
        'NODETAR.blocks': '8' } ]

    , [ 'entry',
      { path: 'fixtures/a.txt',
        mode: 420,
        uid: uid,
        gid: gid,
        size: 257,
        mtime: new Date('Mon, 24 Oct 2011 22:04:11 GMT'),
        cksum: 5114,
        type: '0',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'entry',
      { path: 'fixtures/b.txt',
        mode: 420,
        uid: uid,
        gid: gid,
        size: 512,
        mtime: new Date('Mon, 24 Oct 2011 22:07:59 GMT'),
        cksum: 5122,
        type: '0',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'entry',
      { path: 'fixtures/c.txt',
        mode: 420,
        uid: uid,
        gid: gid,
        size: 513,
        mtime: new Date('Wed, 26 Oct 2011 01:10:58 GMT'),
        cksum: 5119,
        type: '0',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'entry',
      { path: 'fixtures/cc.txt',
        mode: 420,
        uid: uid,
        gid: gid,
        size: 513,
        mtime: new Date('Wed, 26 Oct 2011 01:11:02 GMT'),
        cksum: 5222,
        type: '0',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'entry',
      { path: 'fixtures/foo.js',
        mode: 420,
        uid: uid,
        gid: gid,
        size: 4,
        mtime: new Date('Fri, 21 Oct 2011 21:19:29 GMT'),
        cksum: 5211,
        type: '0',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'entry',
      { path: 'fixtures/hardlink-1',
        mode: 420,
        uid: uid,
        gid: gid,
        size: 200,
        mtime: new Date('Tue, 15 Nov 2011 03:10:09 GMT'),
        cksum: 5554,
        type: '0',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'entry',
      { path: 'fixtures/hardlink-2',
        mode: 420,
        uid: uid,
        gid: gid,
        size: 0,
        mtime: new Date('Tue, 15 Nov 2011 03:10:09 GMT'),
        cksum: 7428,
        type: '1',
        linkpath: 'fixtures/hardlink-1',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'entry',
      { path: 'fixtures/omega.txt',
        mode: 420,
        uid: uid,
        gid: gid,
        size: 2,
        mtime: new Date('Fri, 21 Oct 2011 21:19:29 GMT'),
        cksum: 5537,
        type: '0',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'entry',
      { path: 'fixtures/packtest/',
        mode: 493,
        uid: uid,
        gid: gid,
        size: 0,
        type: '5',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'entry',
      { path: 'fixtures/packtest/omega.txt',
        mode: 420,
        uid: uid,
        gid: gid,
        size: 2,
        mtime: new Date('Mon, 14 Nov 2011 21:42:24 GMT'),
        cksum: 6440,
        type: '0',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'entry',
      { path: 'fixtures/packtest/star.4.html',
        mode: 420,
        uid: uid,
        gid: gid,
        size: 54081,
        mtime: new Date("Sun, 06 May 2007 13:25:06 GMT"),
        cksum: 6566,
        type: '0',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'extendedHeader',
      { path: 'PaxHeader/fixtures/packtest/Ω.txt',
        mode: 420,
        uid: uid,
        gid: gid,
        size: 213,
        mtime: new Date('Mon, 14 Nov 2011 21:39:39 GMT'),
        cksum: 7306,
        type: 'x',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' },
      { path: 'fixtures/packtest/Ω.txt',
        'NODETAR.depth': '2',
        'NODETAR.type': 'File',
        nlink: 1,
        uid: uid,
        gid: gid,
        size: 2,
        'NODETAR.blksize': '4096',
        'NODETAR.blocks': '8' } ]

    , [ 'entry',
      { path: 'fixtures/packtest/Ω.txt',
        mode: 420,
        uid: uid,
        gid: gid,
        size: 2,
        mtime: new Date('Mon, 14 Nov 2011 21:39:39 GMT'),
        cksum: 6297,
        type: '0',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '',
        'NODETAR.depth': '2',
        'NODETAR.type': 'File',
        nlink: 1,
        'NODETAR.blksize': '4096',
        'NODETAR.blocks': '8' } ]

    , [ 'entry',
      { path: 'fixtures/r/',
        mode: 493,
        uid: uid,
        gid: gid,
        size: 0,
        mtime: new Date('Thu, 27 Oct 2011 03:42:46 GMT'),
        cksum: 4789,
        type: '5',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'entry',
      { path: 'fixtures/r/e/',
        mode: 493,
        uid: uid,
        gid: gid,
        size: 0,
        mtime: new Date('Thu, 27 Oct 2011 03:42:46 GMT'),
        cksum: 4937,
        type: '5',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'entry',
      { path: 'fixtures/r/e/a/',
        mode: 493,
        uid: uid,
        gid: gid,
        size: 0,
        mtime: new Date('Thu, 27 Oct 2011 03:42:46 GMT'),
        cksum: 5081,
        type: '5',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'entry',
      { path: 'fixtures/r/e/a/l/',
        mode: 493,
        uid: uid,
        gid: gid,
        size: 0,
        mtime: new Date('Thu, 27 Oct 2011 03:42:46 GMT'),
        cksum: 5236,
        type: '5',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'entry',
      { path: 'fixtures/r/e/a/l/l/',
        mode: 493,
        uid: uid,
        gid: gid,
        size: 0,
        mtime: new Date('Thu, 27 Oct 2011 03:42:46 GMT'),
        cksum: 5391,
        type: '5',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'entry',
      { path: 'fixtures/r/e/a/l/l/y/',
        mode: 493,
        uid: uid,
        gid: gid,
        size: 0,
        mtime: new Date('Thu, 27 Oct 2011 03:42:46 GMT'),
        cksum: 5559,
        type: '5',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'entry',
      { path: 'fixtures/r/e/a/l/l/y/-/',
        mode: 493,
        uid: uid,
        gid: gid,
        size: 0,
        mtime: new Date('Thu, 27 Oct 2011 03:42:46 GMT'),
        cksum: 5651,
        type: '5',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'entry',
      { path: 'fixtures/r/e/a/l/l/y/-/d/',
        mode: 493,
        uid: uid,
        gid: gid,
        size: 0,
        mtime: new Date('Thu, 27 Oct 2011 03:42:46 GMT'),
        cksum: 5798,
        type: '5',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'entry',
      { path: 'fixtures/r/e/a/l/l/y/-/d/e/',
        mode: 493,
        uid: uid,
        gid: gid,
        size: 0,
        mtime: new Date('Thu, 27 Oct 2011 03:42:46 GMT'),
        cksum: 5946,
        type: '5',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'entry',
      { path: 'fixtures/r/e/a/l/l/y/-/d/e/e/',
        mode: 493,
        uid: uid,
        gid: gid,
        size: 0,
        mtime: new Date('Thu, 27 Oct 2011 03:42:46 GMT'),
        cksum: 6094,
        type: '5',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'entry',
      { path: 'fixtures/r/e/a/l/l/y/-/d/e/e/p/',
        mode: 493,
        uid: uid,
        gid: gid,
        size: 0,
        mtime: new Date('Thu, 27 Oct 2011 03:42:46 GMT'),
        cksum: 6253,
        type: '5',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'entry',
      { path: 'fixtures/r/e/a/l/l/y/-/d/e/e/p/-/',
        mode: 493,
        uid: uid,
        gid: gid,
        size: 0,
        mtime: new Date('Thu, 27 Oct 2011 03:42:46 GMT'),
        cksum: 6345,
        type: '5',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'entry',
      { path: 'fixtures/r/e/a/l/l/y/-/d/e/e/p/-/f/',
        mode: 493,
        uid: uid,
        gid: gid,
        size: 0,
        mtime: new Date('Thu, 27 Oct 2011 03:42:46 GMT'),
        cksum: 6494,
        type: '5',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'entry',
      { path: 'fixtures/r/e/a/l/l/y/-/d/e/e/p/-/f/o/',
        mode: 493,
        uid: uid,
        gid: gid,
        size: 0,
        mtime: new Date('Thu, 27 Oct 2011 03:42:46 GMT'),
        cksum: 6652,
        type: '5',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'entry',
      { path: 'fixtures/r/e/a/l/l/y/-/d/e/e/p/-/f/o/l/',
        mode: 493,
        uid: uid,
        gid: gid,
        size: 0,
        mtime: new Date('Thu, 27 Oct 2011 03:42:46 GMT'),
        cksum: 6807,
        type: '5',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'entry',
      { path: 'fixtures/r/e/a/l/l/y/-/d/e/e/p/-/f/o/l/d/',
        mode: 493,
        uid: uid,
        gid: gid,
        size: 0,
        mtime: new Date('Thu, 27 Oct 2011 03:42:46 GMT'),
        cksum: 6954,
        type: '5',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'entry',
      { path: 'fixtures/r/e/a/l/l/y/-/d/e/e/p/-/f/o/l/d/e/',
        mode: 493,
        uid: uid,
        gid: gid,
        size: 0,
        mtime: new Date('Thu, 27 Oct 2011 03:42:46 GMT'),
        cksum: 7102,
        type: '5',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'entry',
      { path: 'fixtures/r/e/a/l/l/y/-/d/e/e/p/-/f/o/l/d/e/r/',
        mode: 493,
        uid: uid,
        gid: gid,
        size: 0,
        mtime: new Date('Thu, 27 Oct 2011 03:42:46 GMT'),
        cksum: 7263,
        type: '5',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'entry',
      { path: 'fixtures/r/e/a/l/l/y/-/d/e/e/p/-/f/o/l/d/e/r/-/',
        mode: 493,
        uid: uid,
        gid: gid,
        size: 0,
        mtime: new Date('Thu, 27 Oct 2011 03:42:46 GMT'),
        cksum: 7355,
        type: '5',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'entry',
      { path: 'fixtures/r/e/a/l/l/y/-/d/e/e/p/-/f/o/l/d/e/r/-/p/',
        mode: 493,
        uid: uid,
        gid: gid,
        size: 0,
        mtime: new Date('Thu, 27 Oct 2011 03:42:46 GMT'),
        cksum: 7514,
        type: '5',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'entry',
      { path: 'fixtures/r/e/a/l/l/y/-/d/e/e/p/-/f/o/l/d/e/r/-/p/a/',
        mode: 493,
        uid: uid,
        gid: gid,
        size: 0,
        mtime: new Date('Thu, 27 Oct 2011 03:42:46 GMT'),
        cksum: 7658,
        type: '5',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'entry',
      { path: 'fixtures/r/e/a/l/l/y/-/d/e/e/p/-/f/o/l/d/e/r/-/p/a/t/',
        mode: 493,
        uid: uid,
        gid: gid,
        size: 0,
        mtime: new Date('Thu, 27 Oct 2011 03:42:46 GMT'),
        cksum: 7821,
        type: '5',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'entry',
      { path: 'fixtures/r/e/a/l/l/y/-/d/e/e/p/-/f/o/l/d/e/r/-/p/a/t/h/',
        mode: 493,
        uid: uid,
        gid: gid,
        size: 0,
        mtime: new Date('Thu, 27 Oct 2011 03:43:23 GMT'),
        cksum: 7967,
        type: '5',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'entry',
      { path: 'fixtures/r/e/a/l/l/y/-/d/e/e/p/-/f/o/l/d/e/r/-/p/a/t/h/cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc',
        mode: 420,
        uid: uid,
        gid: gid,
        size: 100,
        mtime: new Date('Thu, 27 Oct 2011 03:43:23 GMT'),
        cksum: 17821,
        type: '0',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'entry',
      { path: 'fixtures/symlink',
        mode: 493,
        uid: uid,
        gid: gid,
        size: 0,
        mtime: new Date('Tue, 15 Nov 2011 19:57:48 GMT'),
        cksum: 6337,
        type: '2',
        linkpath: 'hardlink-1',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' } ]

    , [ 'extendedHeader',
      { path: 'PaxHeader/fixtures/Ω.txt',
        mode: 420,
        uid: uid,
        gid: gid,
        size: 204,
        mtime: new Date('Thu, 27 Oct 2011 17:51:49 GMT'),
        cksum: 6399,
        type: 'x',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '' },
      { path: "fixtures/Ω.txt"
      , "NODETAR.depth": "1"
      , "NODETAR.type": "File"
      , nlink: 1
      , uid: uid
      , gid: gid
      , size: 2
      , "NODETAR.blksize": "4096"
      , "NODETAR.blocks": "8" } ]

    , [ 'entry',
      { path: 'fixtures/Ω.txt',
        mode: 420,
        uid: uid,
        gid: gid,
        size: 2,
        mtime: new Date('Thu, 27 Oct 2011 17:51:49 GMT'),
        cksum: 5392,
        type: '0',
        linkpath: '',
        ustar: 'ustar\u0000',
        ustarver: '00',
        uname: '',
        gname: '',
        devmaj: 0,
        devmin: 0,
        fill: '',
        'NODETAR.depth': '1',
        'NODETAR.type': 'File',
        nlink: 1,
        'NODETAR.blksize': '4096',
        'NODETAR.blocks': '8' } ]
    ]


// first, make sure that the hardlinks are actually hardlinks, or this
// won't work.  Git has a way of replacing them with a copy.
var hard1 = path.resolve(__dirname, "fixtures/hardlink-1")
  , hard2 = path.resolve(__dirname, "fixtures/hardlink-2")
  , fs = require("fs")

try { fs.unlinkSync(hard2) } catch (e) {}
fs.linkSync(hard1, hard2)

tap.test("with global header", { timeout: 10000 }, function (t) {
  runTest(t, true)
})

tap.test("without global header", { timeout: 10000 }, function (t) {
  runTest(t, false)
})

function runTest (t, doGH) {
  var reader = Reader({ path: input
                      , filter: function () {
                          return !this.path.match(/\.(tar|hex)$/)
                        }
                      })

  var pack = Pack(doGH ? pkg : null)
  var writer = Writer(target)

  // skip the global header if we're not doing that.
  var entry = doGH ? 0 : 1

  t.ok(reader, "reader ok")
  t.ok(pack, "pack ok")
  t.ok(writer, "writer ok")

  pack.pipe(writer)

  var parse = tar.Parse()
  t.ok(parse, "parser should be ok")

  pack.on("data", function (c) {
    // console.error("PACK DATA")
    t.equal(c.length, 512, "parser should emit data in 512byte blocks")
    parse.write(c)
  })

  pack.on("end", function () {
    // console.error("PACK END")
    t.pass("parser ends")
    parse.end()
  })

  pack.on("error", function (er) {
    t.fail("pack error", er)
  })

  parse.on("error", function (er) {
    t.fail("parse error", er)
  })

  writer.on("error", function (er) {
    t.fail("writer error", er)
  })

  reader.on("error", function (er) {
    t.fail("reader error", er)
  })

  parse.on("*", function (ev, e) {
    var wanted = entries[entry++]
    if (!wanted) {
      t.fail("unexpected event: "+ev)
      return
    }
    t.equal(ev, wanted[0], "event type should be "+wanted[0])
    // if (ev !== wanted[0] || e.path !== wanted[1].path) {
    //   console.error(wanted)
    //   console.error([ev, e.props])
    //   throw "break"
    // }
    t.has(e.props, wanted[1], "properties "+wanted[1].path)
    if (wanted[2]) {
      e.on("end", function () {
        t.has(e.fields, wanted[2], "should get expected fields")
      })
    }
  })

  reader.pipe(pack)

  writer.on("close", function () {
    t.equal(entry, entries.length, "should get all expected entries")
    t.pass("it finished")
    t.end()
  })

}
