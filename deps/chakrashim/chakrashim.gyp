{
  'variables': {
    'library_files': [
      'lib/chakra_shim.js',
    ],
  },

  'targets': [
    {
      'target_name': 'chakrashim',
      'type': '<(library)',

      'dependencies': [
        'chakra_js2c#host',
      ],

      'include_dirs': [
        'include',
        '<(SHARED_INTERMEDIATE_DIR)'
      ],
      'defines': [
        'BUILDING_CHAKRASHIM=1',
      ],
      'conditions': [
        [ 'target_arch=="ia32"', { 'defines': [ '__i386__=1' ] } ],
        [ 'target_arch=="x64"', { 'defines': [ '__x86_64__=1' ] } ],
        [ 'target_arch=="arm"', { 'defines': [ '__arm__=1' ] } ],
        ['node_engine=="chakracore"', {
          'dependencies': [
            'chakracore.gyp:chakracore#host',
          ],
          'export_dependent_settings': [
            'chakracore.gyp:chakracore#host',
          ],
        }],
      ],
      'msvs_use_library_dependency_inputs': 1,

      'direct_dependent_settings': {
        'include_dirs': [
          'include',
        ],
        'defines': [
          'BUILDING_CHAKRASHIM=1',
        ],
        'libraries': [
          '-lole32.lib',
          '-lversion.lib',
          '<@(node_engine_libs)',
        ],
        'conditions': [
          [ 'target_arch=="arm"', {
            'defines': [ '__arm__=1' ]
          }],
        ],
      },

      'sources': [
        'include/libplatform/libplatform.h',
        'include/v8.h',
        'include/v8config.h',
        'include/v8-debug.h',
        'include/v8-platform.h',
        'include/v8-profiler.h',
        'include/v8-version.h',
        'src/jsrtcachedpropertyidref.inc',
        'src/jsrtcontextcachedobj.inc',
        'src/jsrtcontextshim.cc',
        'src/jsrtcontextshim.h',
        'src/jsrtisolateshim.cc',
        'src/jsrtisolateshim.h',
        'src/jsrtpromise.cc',
        'src/jsrtproxyutils.cc',
        'src/jsrtproxyutils.h',
        'src/jsrtstringutils.cc',
        'src/jsrtstringutils.h',
        'src/jsrtutils.cc',
        'src/jsrtutils.h',
        'src/v8array.cc',
        'src/v8arraybuffer.cc',
        'src/v8boolean.cc',
        'src/v8booleanobject.cc',
        'src/v8chakra.h',
        'src/v8context.cc',
        'src/v8date.cc',
        'src/v8debug.cc',
        'src/v8exception.cc',
        'src/v8external.cc',
        'src/v8function.cc',
        'src/v8functiontemplate.cc',
        'src/v8global.cc',
        'src/v8handlescope.cc',
        'src/v8int32.cc',
        'src/v8integer.cc',
        'src/v8isolate.cc',
        'src/v8message.cc',
        'src/v8number.cc',
        'src/v8numberobject.cc',
        'src/v8object.cc',
        'src/v8objecttemplate.cc',
        'src/v8persistent.cc',
        'src/v8script.cc',
        'src/v8signature.cc',
        'src/v8stacktrace.cc',
        'src/v8string.cc',
        'src/v8stringobject.cc',
        'src/v8template.cc',
        'src/v8trycatch.cc',
        'src/v8typedarray.cc',
        'src/v8uint32.cc',
        'src/v8v8.cc',
        'src/v8value.cc',
      ],
    },  # end chakrashim

    {
      'target_name': 'chakra_js2c',
      'type': 'none',
      'toolsets': ['host'],
      'msvs_disabled_warnings': [4091],
      'actions': [
        {
          'action_name': 'chakra_js2c',
          'inputs': [
            '<@(library_files)'
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/chakra_natives.h',
          ],
          'action': [
            '<(python)',
            './../../tools/js2c.py',
            '--namespace=jsrt',
            '<@(_outputs)',
            '<@(_inputs)',
          ],
        },
      ],
    }, # end chakra_js2c
  ],
}
