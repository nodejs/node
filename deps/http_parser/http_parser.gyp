# This file is used with the GYP meta build system.
# http://code.google.com/p/gyp/
# To build try this:
#   svn co http://gyp.googlecode.com/svn/trunk gyp
#   ./gyp/gyp -f make --depth=`pwd` http_parser.gyp 
#   ./out/Debug/test 
{
  'targets': [
    {
      'target_name': 'http_parser',
      'type': '<(library)',
      'include_dirs': [ '.' ],
      'direct_dependent_settings': {
        'include_dirs': [ '.' ],
      },
      'defines': [ 'HTTP_PARSER_STRICT=0' ],
      'sources': [ './http_parser.c', ],
      'msvs_settings': {
        'VCCLCompilerTool': {
          # Compile as C++. http_parser.c is actually C99, but C++ is
          # close enough in this case.
          'CompileAs': 2, # compile as C++
        },
      },
    },
    {
      'target_name': 'test',
      'type': 'executable',
      'dependencies': [ 'http_parser' ],
      'sources': [ 'test.c' ]
    }
  ]
}

