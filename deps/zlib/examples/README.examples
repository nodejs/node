This directory contains examples of the use of zlib and other relevant
programs and documentation.

enough.c
    calculation and justification of ENOUGH parameter in inftrees.h
    - calculates the maximum table space used in inflate tree
      construction over all possible Huffman codes

fitblk.c
    compress just enough input to nearly fill a requested output size
    - zlib isn't designed to do this, but fitblk does it anyway

gun.c
    uncompress a gzip file
    - illustrates the use of inflateBack() for high speed file-to-file
      decompression using call-back functions
    - is approximately twice as fast as gzip -d
    - also provides Unix uncompress functionality, again twice as fast

gzappend.c
    append to a gzip file
    - illustrates the use of the Z_BLOCK flush parameter for inflate()
    - illustrates the use of deflatePrime() to start at any bit

gzjoin.c
    join gzip files without recalculating the crc or recompressing
    - illustrates the use of the Z_BLOCK flush parameter for inflate()
    - illustrates the use of crc32_combine()

gzlog.c
gzlog.h
    efficiently and robustly maintain a message log file in gzip format
    - illustrates use of raw deflate, Z_PARTIAL_FLUSH, deflatePrime(),
      and deflateSetDictionary()
    - illustrates use of a gzip header extra field

zlib_how.html
    painfully comprehensive description of zpipe.c (see below)
    - describes in excruciating detail the use of deflate() and inflate()

zpipe.c
    reads and writes zlib streams from stdin to stdout
    - illustrates the proper use of deflate() and inflate()
    - deeply commented in zlib_how.html (see above)

zran.c
    index a zlib or gzip stream and randomly access it
    - illustrates the use of Z_BLOCK, inflatePrime(), and
      inflateSetDictionary() to provide random access
