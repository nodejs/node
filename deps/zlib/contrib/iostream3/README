These classes provide a C++ stream interface to the zlib library. It allows you
to do things like:

  gzofstream outf("blah.gz");
  outf << "These go into the gzip file " << 123 << endl;

It does this by deriving a specialized stream buffer for gzipped files, which is
the way Stroustrup would have done it. :->

The gzifstream and gzofstream classes were originally written by Kevin Ruland
and made available in the zlib contrib/iostream directory. The older version still
compiles under gcc 2.xx, but not under gcc 3.xx, which sparked the development of
this version.

The new classes are as standard-compliant as possible, closely following the
approach of the standard library's fstream classes. It compiles under gcc versions
3.2 and 3.3, but not under gcc 2.xx. This is mainly due to changes in the standard
library naming scheme. The new version of gzifstream/gzofstream/gzfilebuf differs
from the previous one in the following respects:
- added showmanyc
- added setbuf, with support for unbuffered output via setbuf(0,0)
- a few bug fixes of stream behavior
- gzipped output file opened with default compression level instead of maximum level
- setcompressionlevel()/strategy() members replaced by single setcompression()

The code is provided "as is", with the permission to use, copy, modify, distribute
and sell it for any purpose without fee.

Ludwig Schwardt
<schwardt@sun.ac.za>

DSP Lab
Electrical & Electronic Engineering Department
University of Stellenbosch
South Africa
