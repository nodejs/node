# Async.js

Async is a utility module which provides straight-forward, powerful functions
for working with asynchronous JavaScript. Although originally designed for
use with [node.js](http://nodejs.org), it can also be used directly in the
browser.

Async provides around 20 functions that include the usual 'functional'
suspects (map, reduce, filter, forEach…) as well as some common patterns
for asynchronous flow control (parallel, series, waterfall…). All these
functions assume you follow the node.js convention of providing a single
callback as the last argument of your async function.


## Quick Examples

    async.map(['file1','file2','file3'], fs.stat, function(err, results){
        // results is now an array of stats for each file
    });

    async.filter(['file1','file2','file3'], path.exists, function(results){
        // results now equals an array of the existing files
    });

    async.parallel([
        function(){ ... },
        function(){ ... }
    ], callback);

    async.series([
        function(){ ... },
        function(){ ... }
    ]);

There are many more functions available so take a look at the docs below for a
full list. This module aims to be comprehensive, so if you feel anything is
missing please create a GitHub issue for it.


## Download

Releases are available for download from
[GitHub](http://github.com/caolan/async/downloads).
Alternatively, you can install using Node Package Manager (npm):

    npm install async


__Development:__ [async.js](https://github.com/caolan/async/raw/master/lib/async.js) - 17.5kb Uncompressed

__Production:__ [async.min.js](https://github.com/caolan/async/raw/master/dist/async.min.js) - 1.7kb Packed and Gzipped


## In the Browser

So far its been tested in IE6, IE7, IE8, FF3.6 and Chrome 5. Usage:

    <script type="text/javascript" src="async.js"></script>
    <script type="text/javascript">

        async.map(data, asyncProcess, function(err, results){
            alert(results);
        });

    </script>


## Documentation

### Collections

* [forEach](#forEach)
* [map](#map)
* [filter](#filter)
* [reject](#reject)
* [reduce](#reduce)
* [detect](#detect)
* [sortBy](#sortBy)
* [some](#some)
* [every](#every)
* [concat](#concat)

### Flow Control

* [series](#series)
* [parallel](#parallel)
* [whilst](#whilst)
* [until](#until)
* [waterfall](#waterfall)
* [queue](#queue)
* [auto](#auto)
* [iterator](#iterator)
* [apply](#apply)
* [nextTick](#nextTick)

### Utils

* [memoize](#memoize)
* [log](#log)
* [dir](#dir)
* [noConflict](#noConflict)


## Collections

<a name="forEach" />
### forEach(arr, iterator, callback)

Applies an iterator function to each item in an array, in parallel.
The iterator is called with an item from the list and a callback for when it
has finished. If the iterator passes an error to this callback, the main
callback for the forEach function is immediately called with the error.

Note, that since this function applies the iterator to each item in parallel
there is no guarantee that the iterator functions will complete in order.

__Arguments__

* arr - An array to iterate over.
* iterator(item, callback) - A function to apply to each item in the array.
  The iterator is passed a callback which must be called once it has completed.
* callback(err) - A callback which is called after all the iterator functions
  have finished, or an error has occurred.

__Example__

    // assuming openFiles is an array of file names and saveFile is a function
    // to save the modified contents of that file:

    async.forEach(openFiles, saveFile, function(err){
        // if any of the saves produced an error, err would equal that error
    });

---------------------------------------

<a name="forEachSeries" />
### forEachSeries(arr, iterator, callback)

The same as forEach only the iterator is applied to each item in the array in
series. The next iterator is only called once the current one has completed
processing. This means the iterator functions will complete in order.


---------------------------------------

<a name="map" />
### map(arr, iterator, callback)

Produces a new array of values by mapping each value in the given array through
the iterator function. The iterator is called with an item from the array and a
callback for when it has finished processing. The callback takes 2 arguments, 
an error and the transformed item from the array. If the iterator passes an
error to this callback, the main callback for the map function is immediately
called with the error.

Note, that since this function applies the iterator to each item in parallel
there is no guarantee that the iterator functions will complete in order, however
the results array will be in the same order as the original array.

__Arguments__

* arr - An array to iterate over.
* iterator(item, callback) - A function to apply to each item in the array.
  The iterator is passed a callback which must be called once it has completed
  with an error (which can be null) and a transformed item.
* callback(err, results) - A callback which is called after all the iterator
  functions have finished, or an error has occurred. Results is an array of the
  transformed items from the original array.

__Example__

    async.map(['file1','file2','file3'], fs.stat, function(err, results){
        // results is now an array of stats for each file
    });

---------------------------------------

<a name="mapSeries" />
### mapSeries(arr, iterator, callback)

The same as map only the iterator is applied to each item in the array in
series. The next iterator is only called once the current one has completed
processing. The results array will be in the same order as the original.


---------------------------------------

<a name="filter" />
### filter(arr, iterator, callback)

__Alias:__ select

Returns a new array of all the values which pass an async truth test.
_The callback for each iterator call only accepts a single argument of true or
false, it does not accept an error argument first!_ This is in-line with the
way node libraries work with truth tests like path.exists. This operation is
performed in parallel, but the results array will be in the same order as the
original.

__Arguments__

* arr - An array to iterate over.
* iterator(item, callback) - A truth test to apply to each item in the array.
  The iterator is passed a callback which must be called once it has completed.
* callback(results) - A callback which is called after all the iterator
  functions have finished.

__Example__

    async.filter(['file1','file2','file3'], path.exists, function(results){
        // results now equals an array of the existing files
    });

---------------------------------------

<a name="filterSeries" />
### filterSeries(arr, iterator, callback)

__alias:__ selectSeries

The same as filter only the iterator is applied to each item in the array in
series. The next iterator is only called once the current one has completed
processing. The results array will be in the same order as the original.

---------------------------------------

<a name="reject" />
### reject(arr, iterator, callback)

The opposite of filter. Removes values that pass an async truth test.

---------------------------------------

<a name="rejectSeries" />
### rejectSeries(arr, iterator, callback)

The same as filter, only the iterator is applied to each item in the array
in series.


---------------------------------------

<a name="reduce" />
### reduce(arr, memo, iterator, callback)

__aliases:__ inject, foldl

Reduces a list of values into a single value using an async iterator to return
each successive step. Memo is the initial state of the reduction. This
function only operates in series. For performance reasons, it may make sense to
split a call to this function into a parallel map, then use the normal
Array.prototype.reduce on the results. This function is for situations where
each step in the reduction needs to be async, if you can get the data before
reducing it then its probably a good idea to do so.

__Arguments__

* arr - An array to iterate over.
* memo - The initial state of the reduction.
* iterator(memo, item, callback) - A function applied to each item in the
  array to produce the next step in the reduction. The iterator is passed a
  callback which accepts an optional error as its first argument, and the state
  of the reduction as the second. If an error is passed to the callback, the
  reduction is stopped and the main callback is immediately called with the
  error.
* callback(err, result) - A callback which is called after all the iterator
  functions have finished. Result is the reduced value.

__Example__

    async.reduce([1,2,3], 0, function(memo, item, callback){
        // pointless async:
        process.nextTick(function(){
            callback(null, memo + item)
        });
    }, function(err, result){
        // result is now equal to the last value of memo, which is 6
    });

---------------------------------------

<a name="reduceRight" />
### reduceRight(arr, memo, iterator, callback)

__Alias:__ foldr

Same as reduce, only operates on the items in the array in reverse order.


---------------------------------------

<a name="detect" />
### detect(arr, iterator, callback)

Returns the first value in a list that passes an async truth test. The
iterator is applied in parallel, meaning the first iterator to return true will
fire the detect callback with that result. That means the result might not be
the first item in the original array (in terms of order) that passes the test.

If order within the original array is important then look at detectSeries.

__Arguments__

* arr - An array to iterate over.
* iterator(item, callback) - A truth test to apply to each item in the array.
  The iterator is passed a callback which must be called once it has completed.
* callback(result) - A callback which is called as soon as any iterator returns
  true, or after all the iterator functions have finished. Result will be
  the first item in the array that passes the truth test (iterator) or the
  value undefined if none passed.

__Example__

    async.detect(['file1','file2','file3'], path.exists, function(result){
        // result now equals the first file in the list that exists
    });

---------------------------------------

<a name="detectSeries" />
### detectSeries(arr, iterator, callback)

The same as detect, only the iterator is applied to each item in the array
in series. This means the result is always the first in the original array (in
terms of array order) that passes the truth test.


---------------------------------------

<a name="sortBy" />
### sortBy(arr, iterator, callback)

Sorts a list by the results of running each value through an async iterator.

__Arguments__

* arr - An array to iterate over.
* iterator(item, callback) - A function to apply to each item in the array.
  The iterator is passed a callback which must be called once it has completed
  with an error (which can be null) and a value to use as the sort criteria.
* callback(err, results) - A callback which is called after all the iterator
  functions have finished, or an error has occurred. Results is the items from
  the original array sorted by the values returned by the iterator calls.

__Example__

    async.sortBy(['file1','file2','file3'], function(file, callback){
        fs.stat(file, function(err, stats){
            callback(err, stats.mtime);
        });
    }, function(err, results){
        // results is now the original array of files sorted by
        // modified date
    });


---------------------------------------

<a name="some" />
### some(arr, iterator, callback)

__Alias:__ any

Returns true if at least one element in the array satisfies an async test.
_The callback for each iterator call only accepts a single argument of true or
false, it does not accept an error argument first!_ This is in-line with the
way node libraries work with truth tests like path.exists. Once any iterator
call returns true, the main callback is immediately called.

__Arguments__

* arr - An array to iterate over.
* iterator(item, callback) - A truth test to apply to each item in the array.
  The iterator is passed a callback which must be called once it has completed.
* callback(result) - A callback which is called as soon as any iterator returns
  true, or after all the iterator functions have finished. Result will be
  either true or false depending on the values of the async tests.

__Example__

    async.some(['file1','file2','file3'], path.exists, function(result){
        // if result is true then at least one of the files exists
    });

---------------------------------------

<a name="every" />
### every(arr, iterator, callback)

__Alias:__ all

Returns true if every element in the array satisfies an async test.
_The callback for each iterator call only accepts a single argument of true or
false, it does not accept an error argument first!_ This is in-line with the
way node libraries work with truth tests like path.exists.

__Arguments__

* arr - An array to iterate over.
* iterator(item, callback) - A truth test to apply to each item in the array.
  The iterator is passed a callback which must be called once it has completed.
* callback(result) - A callback which is called after all the iterator
  functions have finished. Result will be either true or false depending on
  the values of the async tests.

__Example__

    async.every(['file1','file2','file3'], path.exists, function(result){
        // if result is true then every file exists
    });

---------------------------------------

<a name="concat" />
### concat(arr, iterator, callback)

Applies an iterator to each item in a list, concatenating the results. Returns the
concatenated list. The iterators are called in parallel, and the results are
concatenated as they return. There is no guarantee that the results array will
be returned in the original order of the arguments passed to the iterator function.

__Arguments__

* arr - An array to iterate over
* iterator(item, callback) - A function to apply to each item in the array.
  The iterator is passed a callback which must be called once it has completed
  with an error (which can be null) and an array of results.
* callback(err, results) - A callback which is called after all the iterator
  functions have finished, or an error has occurred. Results is an array containing
  the concatenated results of the iterator function.

__Example__

    async.concat(['dir1','dir2','dir3'], fs.readdir, function(err, files){
        // files is now a list of filenames that exist in the 3 directories
    });

---------------------------------------

<a name="concatSeries" />
### concatSeries(arr, iterator, callback)

Same as async.concat, but executes in series instead of parallel.


## Flow Control

<a name="series" />
### series(tasks, [callback])

Run an array of functions in series, each one running once the previous
function has completed. If any functions in the series pass an error to its
callback, no more functions are run and the callback for the series is
immediately called with the value of the error. Once the tasks have completed,
the results are passed to the final callback as an array.

It is also possible to use an object instead of an array. Each property will be
run as a function and the results will be passed to the final callback as an object
instead of an array. This can be a more readable way of handling results from
async.series.


__Arguments__

* tasks - An array or object containing functions to run, each function is passed
  a callback it must call on completion.
* callback(err, results) - An optional callback to run once all the functions
  have completed. This function gets an array of all the arguments passed to
  the callbacks used in the array.

__Example__

    async.series([
        function(callback){
            // do some stuff ...
            callback(null, 'one');
        },
        function(callback){
            // do some more stuff ...
            callback(null, 'two');
        },
    ],
    // optional callback
    function(err, results){
        // results is now equal to ['one', 'two']
    });


    // an example using an object instead of an array
    async.series({
        one: function(callback){
            setTimeout(function(){
                callback(null, 1);
            }, 200);
        },
        two: function(callback){
            setTimeout(function(){
                callback(null, 2);
            }, 100);
        },
    },
    function(err, results) {
        // results is now equals to: {one: 1, two: 2}
    });


---------------------------------------

<a name="parallel" />
### parallel(tasks, [callback])

Run an array of functions in parallel, without waiting until the previous
function has completed. If any of the functions pass an error to its
callback, the main callback is immediately called with the value of the error.
Once the tasks have completed, the results are passed to the final callback as an
array.

It is also possible to use an object instead of an array. Each property will be
run as a function and the results will be passed to the final callback as an object
instead of an array. This can be a more readable way of handling results from
async.parallel.


__Arguments__

* tasks - An array or object containing functions to run, each function is passed a
  callback it must call on completion.
* callback(err, results) - An optional callback to run once all the functions
  have completed. This function gets an array of all the arguments passed to
  the callbacks used in the array.

__Example__

    async.parallel([
        function(callback){
            setTimeout(function(){
                callback(null, 'one');
            }, 200);
        },
        function(callback){
            setTimeout(function(){
                callback(null, 'two');
            }, 100);
        },
    ],
    // optional callback
    function(err, results){
        // in this case, the results array will equal ['two','one']
        // because the functions were run in parallel and the second
        // function had a shorter timeout before calling the callback.
    });


    // an example using an object instead of an array
    async.parallel({
        one: function(callback){
            setTimeout(function(){
                callback(null, 1);
            }, 200);
        },
        two: function(callback){
            setTimeout(function(){
                callback(null, 2);
            }, 100);
        },
    },
    function(err, results) {
        // results is now equals to: {one: 1, two: 2}
    });


---------------------------------------

<a name="whilst" />
### whilst(test, fn, callback)

Repeatedly call fn, while test returns true. Calls the callback when stopped,
or an error occurs.

__Arguments__

* test() - synchronous truth test to perform before each execution of fn.
* fn(callback) - A function to call each time the test passes. The function is
  passed a callback which must be called once it has completed with an optional
  error as the first argument.
* callback(err) - A callback which is called after the test fails and repeated
  execution of fn has stopped.

__Example__

    var count = 0;

    async.whilst(
        function () { return count < 5; },
        function (callback) {
            count++;
            setTimeout(callback, 1000);
        },
        function (err) {
            // 5 seconds have passed
        }
    });


---------------------------------------

<a name="until" />
### until(test, fn, callback)

Repeatedly call fn, until test returns true. Calls the callback when stopped,
or an error occurs.

The inverse of async.whilst.


---------------------------------------

<a name="waterfall" />
### waterfall(tasks, [callback])

Runs an array of functions in series, each passing their results to the next in
the array. However, if any of the functions pass an error to the callback, the
next function is not executed and the main callback is immediately called with
the error.

__Arguments__

* tasks - An array of functions to run, each function is passed a callback it
  must call on completion.
* callback(err) - An optional callback to run once all the functions have
  completed. This function gets passed any error that may have occurred.

__Example__

    async.waterfall([
        function(callback){
            callback(null, 'one', 'two');
        },
        function(arg1, arg2, callback){
            callback(null, 'three');
        },
        function(arg1, callback){
            // arg1 now equals 'three'
            callback(null, 'done');
        }
    ]);


---------------------------------------

<a name="queue" />
### queue(worker, concurrency)

Creates a queue object with the specified concurrency. Tasks added to the
queue will be processed in parallel (up to the concurrency limit). If all
workers are in progress, the task is queued until one is available. Once
a worker has completed a task, the task's callback is called.

__Arguments__

* worker(task, callback) - An asynchronous function for processing a queued
  task.
* concurrency - An integer for determining how many worker functions should be
  run in parallel.

__Queue objects__

The queue object returned by this function has the following properties and
methods:

* length() - a function returning the number of items waiting to be processed.
* concurrency - an integer for determining how many worker functions should be
  run in parallel. This property can be changed after a queue is created to
  alter the concurrency on-the-fly.
* push(task, [callback]) - add a new task to the queue, the callback is called
  once the worker has finished processing the task.
* saturated - a callback that is called when the queue length hits the concurrency and further tasks will be queued
* empty - a callback that is called when the last item from the queue is given to a worker
* drain - a callback that is called when the last item from the queue has returned from the worker

__Example__

    // create a queue object with concurrency 2

    var q = async.queue(function (task, callback) {
        console.log('hello ' + task.name).
        callback();
    }, 2);


    // assign a callback
    q.drain = function() {
        console.log('all items have been processed');
    }

    // add some items to the queue

    q.push({name: 'foo'}, function (err) {
        console.log('finished processing foo');
    });
    q.push({name: 'bar'}, function (err) {
        console.log('finished processing bar');
    });


---------------------------------------

<a name="auto" />
### auto(tasks, [callback])

Determines the best order for running functions based on their requirements.
Each function can optionally depend on other functions being completed first,
and each function is run as soon as its requirements are satisfied. If any of
the functions pass and error to their callback, that function will not complete
(so any other functions depending on it will not run) and the main callback
will be called immediately with the error.

__Arguments__

* tasks - An object literal containing named functions or an array of
  requirements, with the function itself the last item in the array. The key
  used for each function or array is used when specifying requirements. The
  syntax is easier to understand by looking at the example.
* callback(err) - An optional callback which is called when all the tasks have
  been completed. The callback may receive an error as an argument.

__Example__

    async.auto({
        get_data: function(callback){
            // async code to get some data
        },
        make_folder: function(callback){
            // async code to create a directory to store a file in
            // this is run at the same time as getting the data
        },
        write_file: ['get_data', 'make_folder', function(callback){
            // once there is some data and the directory exists,
            // write the data to a file in the directory
        }],
        email_link: ['write_file', function(callback){
            // once the file is written let's email a link to it...
        }]
    });

This is a fairly trivial example, but to do this using the basic parallel and
series functions would look like this:

    async.parallel([
        function(callback){
            // async code to get some data
        },
        function(callback){
            // async code to create a directory to store a file in
            // this is run at the same time as getting the data
        }
    ],
    function(results){
        async.series([
            function(callback){
                // once there is some data and the directory exists,
                // write the data to a file in the directory
            },
            email_link: ['write_file', function(callback){
                // once the file is written let's email a link to it...
            }
        ]);
    });

For a complicated series of async tasks using the auto function makes adding
new tasks much easier and makes the code more readable. 


---------------------------------------

<a name="iterator" />
### iterator(tasks)

Creates an iterator function which calls the next function in the array,
returning a continuation to call the next one after that. Its also possible to
'peek' the next iterator by doing iterator.next().

This function is used internally by the async module but can be useful when
you want to manually control the flow of functions in series.

__Arguments__

* tasks - An array of functions to run, each function is passed a callback it
  must call on completion.

__Example__

    var iterator = async.iterator([
        function(){ sys.p('one'); },
        function(){ sys.p('two'); },
        function(){ sys.p('three'); }
    ]);

    node> var iterator2 = iterator();
    'one'
    node> var iterator3 = iterator2();
    'two'
    node> iterator3();
    'three'
    node> var nextfn = iterator2.next();
    node> nextfn();
    'three'


---------------------------------------

<a name="apply" />
### apply(function, arguments..)

Creates a continuation function with some arguments already applied, a useful
shorthand when combined with other flow control functions. Any arguments
passed to the returned function are added to the arguments originally passed
to apply.

__Arguments__

* function - The function you want to eventually apply all arguments to.
* arguments... - Any number of arguments to automatically apply when the
  continuation is called.

__Example__

    // using apply

    async.parallel([
        async.apply(fs.writeFile, 'testfile1', 'test1'),
        async.apply(fs.writeFile, 'testfile2', 'test2'),
    ]);


    // the same process without using apply

    async.parallel([
        function(callback){
            fs.writeFile('testfile1', 'test1', callback);
        },
        function(callback){
            fs.writeFile('testfile2', 'test2', callback);
        },
    ]);

It's possible to pass any number of additional arguments when calling the
continuation:

    node> var fn = async.apply(sys.puts, 'one');
    node> fn('two', 'three');
    one
    two
    three

---------------------------------------

<a name="nextTick" />
### nextTick(callback)

Calls the callback on a later loop around the event loop. In node.js this just
calls process.nextTick, in the browser it falls back to setTimeout(callback, 0),
which means other higher priority events may precede the execution of the callback.

This is used internally for browser-compatibility purposes.

__Arguments__

* callback - The function to call on a later loop around the event loop.

__Example__

    var call_order = [];
    async.nextTick(function(){
        call_order.push('two');
        // call_order now equals ['one','two]
    });
    call_order.push('one')


## Utils

<a name="memoize" />
### memoize(fn, [hasher])

Caches the results of an async function. When creating a hash to store function
results against, the callback is omitted from the hash and an optional hash
function can be used.

__Arguments__

* fn - the function you to proxy and cache results from.
* hasher - an optional function for generating a custom hash for storing
  results, it has all the arguments applied to it apart from the callback, and
  must be synchronous.

__Example__

    var slow_fn = function (name, callback) {
        // do something
        callback(null, result);
    };
    var fn = async.memoize(slow_fn);

    // fn can now be used as if it were slow_fn
    fn('some name', function () {
        // callback
    });


<a name="log" />
### log(function, arguments)

Logs the result of an async function to the console. Only works in node.js or
in browsers that support console.log and console.error (such as FF and Chrome).
If multiple arguments are returned from the async function, console.log is
called on each argument in order.

__Arguments__

* function - The function you want to eventually apply all arguments to.
* arguments... - Any number of arguments to apply to the function.

__Example__

    var hello = function(name, callback){
        setTimeout(function(){
            callback(null, 'hello ' + name);
        }, 1000);
    };

    node> async.log(hello, 'world');
    'hello world'


---------------------------------------

<a name="dir" />
### dir(function, arguments)

Logs the result of an async function to the console using console.dir to
display the properties of the resulting object. Only works in node.js or
in browsers that support console.dir and console.error (such as FF and Chrome).
If multiple arguments are returned from the async function, console.dir is
called on each argument in order.

__Arguments__

* function - The function you want to eventually apply all arguments to.
* arguments... - Any number of arguments to apply to the function.

__Example__

    var hello = function(name, callback){
        setTimeout(function(){
            callback(null, {hello: name});
        }, 1000);
    };

    node> async.dir(hello, 'world');
    {hello: 'world'}


---------------------------------------

<a name="noConflict" />
### noConflict()

Changes the value of async back to its original value, returning a reference to the
async object.
