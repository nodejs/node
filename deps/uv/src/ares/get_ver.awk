# ***************************************************************************
# *  Project: c-ares
# *
# ***************************************************************************
# awk script which fetches c-ares version number and string from input
# file and writes them to STDOUT. Here you can get an awk version for Win32:
# http://www.gknw.net/development/prgtools/awk-20070501.zip
#
BEGIN {
  if (match (ARGV[1], /ares_version.h/)) {
    while ((getline < ARGV[1]) > 0) {
      if (match ($0, /^#define ARES_COPYRIGHT "[^"]+"$/)) {
        libcares_copyright_str = substr($0, 25, length($0)-25);
      }
      else if (match ($0, /^#define ARES_VERSION_STR "[^"]+"$/)) {
        libcares_ver_str = substr($3, 2, length($3)-2);
      }
      else if (match ($0, /^#define ARES_VERSION_MAJOR [0-9]+$/)) {
        libcares_ver_major = substr($3, 1, length($3));
      }
      else if (match ($0, /^#define ARES_VERSION_MINOR [0-9]+$/)) {
        libcares_ver_minor = substr($3, 1, length($3));
      }
      else if (match ($0, /^#define ARES_VERSION_PATCH [0-9]+$/)) {
        libcares_ver_patch = substr($3, 1, length($3));
      }
    }
    libcares_ver = libcares_ver_major "," libcares_ver_minor "," libcares_ver_patch;
    print "LIBCARES_VERSION = " libcares_ver "";
    print "LIBCARES_VERSION_STR = " libcares_ver_str "";
    print "LIBCARES_COPYRIGHT_STR = " libcares_copyright_str "";
  }
}


