
0.7.1 / 2012-03-26 
==================

  * Fixed exception when using express in production caused by typo. [slaskis]

0.7.0 / 2012-03-24 
==================

  * Added newline consumption support (`-%>`) [whoatemydomain]

0.6.1 / 2011-12-09 
==================

  * Fixed `ejs.renderFile()`

0.6.0 / 2011-12-09 
==================

  * Changed: you no longer need `{ locals: {} }`

0.5.0 / 2011-11-20 
==================

  * Added express 3.x support
  * Added ejs.renderFile()
  * Added 'json' filter
  * Fixed tests for 0.5.x

0.4.3 / 2011-06-20 
==================

  * Fixed stacktraces line number when used multiline js expressions [Octave]

0.4.2 / 2011-05-11 
==================

  * Added client side support

0.4.1 / 2011-04-21 
==================

  * Fixed error context

0.4.0 / 2011-04-21 
==================

  * Added; ported jade's error reporting to ejs. [slaskis]

0.3.1 / 2011-02-23 
==================

  * Fixed optional `compile()` options

0.3.0 / 2011-02-14 
==================

  * Added 'json' filter [Yuriy Bogdanov]
  * Use exported version of parse function to allow monkey-patching [Anatoliy Chakkaev]

0.2.1 / 2010-10-07 
==================

  * Added filter support
  * Fixed _cache_ option. ~4x performance increase

0.2.0 / 2010-08-05 
==================

  * Added support for global tag config
  * Added custom tag support. Closes #5
  * Fixed whitespace bug. Closes #4

0.1.0 / 2010-08-04
==================

  * Faster implementation [ashleydev]

0.0.4 / 2010-08-02
==================

  * Fixed single quotes for content outside of template tags. [aniero]
  * Changed; `exports.compile()` now expects only "locals"

0.0.3 / 2010-07-15
==================

  * Fixed single quotes

0.0.2 / 2010-07-09
==================

  * Fixed newline preservation

0.0.1 / 2010-07-09
==================

  * Initial release
