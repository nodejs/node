// field names that every tar file must have.
// header is padded to 512 bytes.
var f = 0
  , fields = {}
  , NAME = fields.NAME = f++
  , MODE = fields.MODE = f++
  , UID = fields.UID = f++
  , GID = fields.GID = f++
  , SIZE = fields.SIZE = f++
  , MTIME = fields.MTIME = f++
  , CKSUM = fields.CKSUM = f++
  , TYPE = fields.TYPE = f++
  , LINKNAME = fields.LINKNAME = f++
  , headerSize = 512
  , fieldSize = []

fieldSize[NAME] = 100
fieldSize[MODE] = 8
fieldSize[UID] = 8
fieldSize[GID] = 8
fieldSize[SIZE] = 12
fieldSize[MTIME] = 12
fieldSize[CKSUM] = 8
fieldSize[TYPE] = 1
fieldSize[LINKNAME] = 100

// "ustar\0" may introduce another bunch of headers.
// these are optional, and will be nulled out if not present.
var ustar = new Buffer(6)
ustar.asciiWrite("ustar\0")

var USTAR = fields.USTAR = f++
  , USTARVER = fields.USTARVER = f++
  , UNAME = fields.UNAME = f++
  , GNAME = fields.GNAME = f++
  , DEVMAJ = fields.DEVMAJ = f++
  , DEVMIN = fields.DEVMIN = f++
  , PREFIX = fields.PREFIX = f++
// terminate fields.
fields[f] = null

fieldSize[USTAR] = 6
fieldSize[USTARVER] = 2
fieldSize[UNAME] = 32
fieldSize[GNAME] = 32
fieldSize[DEVMAJ] = 8
fieldSize[DEVMIN] = 8
fieldSize[PREFIX] = 155

var fieldEnds = {}
  , fieldOffs = {}
  , fe = 0
for (var i = 0; i < f; i ++) {
  fieldOffs[i] = fe
  fieldEnds[i] = (fe += fieldSize[i])
}

// build a translation table of field names.
Object.keys(fields).forEach(function (f) {
  fields[fields[f]] = f
})

exports.ustar = ustar
exports.fields = fields
exports.fieldSize = fieldSize
exports.fieldOffs = fieldOffs
exports.fieldEnds = fieldEnds
exports.headerSize = headerSize

var Parser = exports.Parser = require("./parser")
exports.createParser = Parser.create

var Generator = exports.Generator = require("./generator")
exports.createGenerator = Generator.create
