# ***************************************************************************
# *  Project: c-ares
# *
# ***************************************************************************
# awk script which fetches c-ares version number and string from input
# file and writes them to STDOUT. Here you can get an awk version for Win32:
# http://www.gknw.net/development/prgtools/awk-20100523.zip
#
BEGIN {
  while ((getline < ARGV[1]) > 0) {
    sub("\r", "") # make MSYS gawk work with CRLF header input.
    if (match ($0, /^#define ARES_COPYRIGHT "[^"]+"$/))
      copyright_string = substr($0, 25, length($0)-25)
    else if (match ($0, /^#define ARES_VERSION_STR "[^"]+"$/))
      version_string = substr($3, 2, length($3)-2)
    else if (match ($0, /^#define ARES_VERSION_MAJOR [0-9]+$/))
      version_major = $3
    else if (match ($0, /^#define ARES_VERSION_MINOR [0-9]+$/))
      version_minor = $3
    else if (match ($0, /^#define ARES_VERSION_PATCH [0-9]+$/))
      version_patch = $3
  }
  print "LIBCARES_VERSION = " version_major "," version_minor "," version_patch
  print "LIBCARES_VERSION_STR = " version_string
  print "LIBCARES_COPYRIGHT_STR = " copyright_string
}
