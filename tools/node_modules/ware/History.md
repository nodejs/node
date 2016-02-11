
1.3.0 - April 30, 2015
----------------------
* throw on errors without a `done` callback
* add `files` to `package.json`

1.2.0 - September 25, 2014
--------------------------
* move `co` to `devDependencies`
* add client-side support

1.1.0 - September 19, 2014
--------------------------
* refactor to use `wrap-fn`

1.0.1 - August 26, 2014
-----------------------
* add support for passing an array of ware instances

1.0.0 - August 26, 2014
-----------------------
* add support for sync middleware
* add support for generators
* remove support for error middleware with arity

0.3.0 - April 24, 2014
----------------------
* let `use` accept arrays
* let `use` accept `Ware` instances
* allow passing `fns` to the constructor

0.2.2 - March 18, 2013
----------------------
* moving to settimeout to fix dom validation issues
* add travis

0.2.1 - December 12, 2013
-------------------------
* fix skipping error handlers without error

0.2.0 - October 18, 2013
------------------------
* rewrite:
  * handle any amount of input arguments
  * allow multiple error handlers
  * only call error handlers after the error is `next`'d
  * make passing callback optional
  * rename `end` to `run` since it can happen multiple times

0.1.0 - July 29, 2013
---------------------
* updating api

0.0.1 - July 29, 2013
---------------------
* first commit
