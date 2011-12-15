
module.exports = relativize

// return the shortest path between two folders.
// if the original path is shorter, then use that,
// unless forceRelative is set to true.
var path = require("path")
function relativize (dest, src, forceRelative) {
  var orig = dest
  if (!isAbsolute(src)) forceRelative = true
  else if (!isAbsolute(dest)) return false
  src = path.resolve(src)
  dest = path.resolve(dest)
  if (src === dest) return "."
  src = src.split(split)
  dest = dest.split(split)
  var i = 0
  while (src[i] === dest[i]) i++
  if (!forceRelative && i === 1) return orig // nothing in common
  src.splice(0, i + 1)
  var dots = [0, i, "."]
  for (var i = 0, l = src.length; i < l; i ++) dots.push("..")
  dest.splice.apply(dest, dots)
  if (dest[0] === "." && dest[1] === "..") dest.shift()
  dest = dest.join("/")
  return !forceRelative && orig.length < dest.length ? orig : dest
}

var split = process.platform === "win32" ? /[\/\\]/ : "/"

function isAbsolute (p) {
  if (process.platform !== "win32") return p.charAt(0) === "/"


  // yanked from node/lib/path.js
  var splitDeviceRe =
    /^([a-zA-Z]:|[\\\/]{2}[^\\\/]+[\\\/][^\\\/]+)?([\\\/])?([\s\S]*?)$/

  var result = p.match(splitDeviceRe)
    , device = result[1] || ""
    , isUnc = device && device.charAt(1) !== ":"
    , isAbs = !!result[2] || isUnc // UNC always absolute

  return isAbs
}

if (module === require.main) {
  // from, to, result, relativeForced
  var assert = require("assert")

  ; [ ["/bar"        ,"/foo"           ,"/bar"          ,"./bar"         ]
    , ["/foo/baz"    ,"/foo/bar/baz"   ,"../baz"        ,"../baz"        ]
    , ["/a/d"        ,"/a/b/c/d/e/f"   ,"/a/d"          ,"../../../../d" ]
    // trailing slashes are ignored.
    , ["/a/d"        ,"/a/b/c/d/e/"    ,"/a/d"          ,"../../../d"    ]
    , ["./foo/bar"   ,"./foo/baz"      ,"./bar"         ,"./bar"         ]
    // force relative when the src is relative.
    , ["./d"         ,"./a/b/c/d/e"    ,"../../../../d" ,"../../../../d" ]
    // if src is abs and dest is relative, then fail
    , ["./d"        ,"/a/b"            ,false           ,false           ]
    ].forEach(function (test) {
      var d = test[0]
        , s = test[1]
        , r = test[2]
        , rr = test[3]
        , ra = relativize(d, s)
        , rra = relativize(d, s, true)
      console.log([d, s, r, rr], [ra, rra], [r === ra, rr === rra])
      assert.equal(r, ra)
      assert.equal(rr, rra)
      if (!r) return
      // contract: this is the relative path from absolute A to absolute B
      var ad = path.resolve(d)
        , as = path.resolve(s)
        , dir = path.dirname(as)
      assert.equal(path.resolve(dir, rr), ad)
      assert.equal(path.resolve(dir, r), ad)
    })

  console.log("ok")
}
