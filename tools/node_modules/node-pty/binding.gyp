{
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'conpty',
          'include_dirs' : [
            '<!(node -e "require(\'nan\')")'
          ],
          'sources' : [
            'src/win/conpty.cc',
            'src/win/path_util.cc'
          ],
          'libraries': [
            'shlwapi.lib'
          ]
        },
        {
          'target_name': 'conpty_console_list',
          'include_dirs' : [
            '<!(node -e "require(\'nan\')")'
          ],
          'sources' : [
            'src/win/conpty_console_list.cc'
          ]
        },
        {
          'target_name': 'pty',
          'include_dirs' : [
            '<!(node -e "require(\'nan\')")',
            'deps/winpty/src/include',
          ],
          # Disabled due to winpty
          'msvs_disabled_warnings': [ 4506, 4530 ],
          'dependencies' : [
            'deps/winpty/src/winpty.gyp:winpty-agent',
            'deps/winpty/src/winpty.gyp:winpty',
          ],
          'sources' : [
            'src/win/winpty.cc',
            'src/win/path_util.cc'
          ],
          'libraries': [
            'shlwapi.lib'
          ],
        }
      ]
    }, { # OS!="win"
      'targets': [{
        'target_name': 'pty',
        'include_dirs' : [
          '<!(node -e "require(\'nan\')")'
        ],
        'sources': [
          'src/unix/pty.cc'
        ],
        'libraries': [
          '-lutil'
        ],
        'conditions': [
          # http://www.gnu.org/software/gnulib/manual/html_node/forkpty.html
          #   One some systems (at least including Cygwin, Interix,
          #   OSF/1 4 and 5, and Mac OS X) linking with -lutil is not required.
          ['OS=="mac" or OS=="solaris"', {
            'libraries!': [
              '-lutil'
            ]
          }],
          ['OS=="mac"', {
            "xcode_settings": {
              "MACOSX_DEPLOYMENT_TARGET":"10.7"
            }
          }]
        ]
      }]
    }]
  ]
}
