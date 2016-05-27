{
  'targets': [{
    'target_name': 'weakref',
    'sources': [ 'src/weakref.cc' ],
    'include_dirs': [
      '<!(node -e "require(\'nan\')")'
    ]
  }]
}
