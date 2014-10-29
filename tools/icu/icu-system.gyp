# Copyright (c) 2014 IBM Corporation and Others. All Rights Reserved.

# This variant is used for the '--with-intl=system-icu' option.
# 'configure' has already set 'libs' and 'cflags' - so,
# there's nothing to do in these targets.

{
  'targets': [
    {
      'target_name': 'icuuc',
      'type': 'none',
    },
    {
      'target_name': 'icui18n',
      'type': 'none',
    },
  ],
}
