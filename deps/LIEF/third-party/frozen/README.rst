Frozen
######

.. image:: https://travis-ci.org/serge-sans-paille/frozen.svg?branch=master
   :target: https://travis-ci.org/serge-sans-paille/frozen

Header-only library that provides 0 cost initialization for immutable containers, fixed-size containers, and various algorithms.

Frozen provides:

- immutable (a.k.a. frozen), ``constexpr``-compatible versions of ``std::set``,
  ``std::unordered_set``, ``std::map`` and ``std::unordered_map``.
  
- fixed-capacity, ``constinit``-compatible versions of ``std::map`` and 
  ``std::unordered_map`` with immutable, compile-time selected keys mapped
  to mutable values.

- 0-cost initialization version of ``std::search`` for frozen needles using
  Boyer-Moore or Knuth-Morris-Pratt algorithms.


The ``unordered_*`` containers are guaranteed *perfect* (a.k.a. no hash
collision) and the extra storage is linear with respect to the number of keys.

Once initialized, the container keys cannot be updated, and in exchange, lookups
are faster. And initialization is free when ``constexpr`` or ``constinit`` is 
used :-).


Installation
------------

Just copy the ``include/frozen`` directory somewhere and points to it using the ``-I`` flag. Alternatively, using CMake:

.. code:: sh

    > mkdir build
    > cd build
    > cmake -D CMAKE_BUILD_TYPE=Release ..
    > make install


Installation via CMake populates configuration files into the ``/usr/local/share``
directory which can be consumed by CMake's ``find_package`` instrinsic function.

Requirements
------------

A C++ compiler that supports C++14. Clang version 5 is a good pick, GCC version
6 lags behind in terms of ``constexpr`` compilation time (At least on my
setup), but compiles correctly. Visual Studio 2017 also works correctly!

Note that gcc 5 isn't supported. (Here's an `old compat branch`_ where a small amount of stuff was ported.)

.. _old compat branch: https://github.com/cbeck88/frozen/tree/gcc5-support

Usage
-----

Compiled with ``-std=c++14`` flag:

.. code:: C++

    #include <frozen/set.h>

    constexpr frozen::set<int, 4> some_ints = {1,2,3,5};

    constexpr bool letitgo = some_ints.count(8);

    extern int n;
    bool letitgoooooo = some_ints.count(n);


As the constructor and some methods are ``constexpr``, it's also possible to write weird stuff like:

.. code:: C++

    #include <frozen/set.h>

    template<std::size_t N>
    std::enable_if_t< frozen::set<int, 3>{{1,11,111}}.count(N), int> foo();

String support is built-in:

.. code:: C++

    #include <frozen/unordered_map.h>
    #include <frozen/string.h>

    constexpr frozen::unordered_map<frozen::string, int, 2> olaf = {
        {"19", 19},
        {"31", 31},
    };
    constexpr auto val = olaf.at("19");

The associative containers have different functionality with and without ``constexpr``. 
With ``constexpr``, frozen maps have immutable keys and values. Without ``constexpr``, the 
values can be updated in runtime (the keys, however, remain immutable):

.. code:: C++


    #include <frozen/unordered_map.h>
    #include <frozen/string.h>

    static constinit frozen::unordered_map<frozen::string, frozen::string, 2> voice = {
        {"Anna", "???"},
        {"Elsa", "???"}
    };
    
    int main() {
    	voice.at("Anna") = "Kristen";
	voice.at("Elsa") = "Idina";
    }

You may also prefer a slightly more DRY initialization syntax:

.. code:: C++

    #include <frozen/set.h>

    constexpr auto some_ints = frozen::make_set<int>({1,2,3,5});

There are similar ``make_X`` functions for all frozen containers.

Exception Handling
------------------

For compatibility with STL's API, Frozen may eventually throw exceptions, as in
``frozen::map::at``. If you build your code without exception support, or
define the ``FROZEN_NO_EXCEPTIONS`` macro variable, they will be turned into an
``std::abort``.

Extending
---------

Just like the regular C++14 container, you can specialize the hash function,
the key equality comparator for ``unordered_*`` containers, and the comparison
functions for the ordered version.

It's also possible to specialize the ``frozen::elsa`` structure used for
hashing. Note that unlike `std::hash`, the hasher also takes a seed in addition
to the value being hashed.

.. code:: C++

    template <class T> struct elsa {
      // in case of collisions, different seeds are tried
      constexpr std::size_t operator()(T const &value, std::size_t seed) const;
    };

Ideally, the hash function should have nice statistical properties like *pairwise-independence*:

If ``x`` and ``y`` are different values, the chance that ``elsa<T>{}(x, seed) == elsa<T>{}(y, seed)``
should be very low for a random value of ``seed``.

Note that frozen always ultimately produces a perfect hash function, and you will always have ``O(1)``
lookup with frozen. It's just that if the input hasher performs poorly, the search will take longer and
your project will take longer to compile.

Troubleshooting
---------------

If you hit a message like this:

.. code:: none

    [...]
    note: constexpr evaluation hit maximum step limit; possible infinite loop?

Then either you've got a very big container and you should increase Clang's
thresholds, using ``-fconstexpr-steps=1000000000`` for instance, or the hash
functions used by frozen do not suit your data, and you should change them, as
in the following:

.. code:: c++

    struct olaf {
      constexpr std::size_t operator()(frozen::string const &value, std::size_t seed) const { return seed ^ value[0];}
    };

    constexpr frozen::unordered_set<frozen::string, 2, olaf/*custom hash*/> hans = { "a", "b" };

Tests and Benchmarks
--------------------

Using hand-written Makefiles crafted with love and care:

.. code:: sh

    > # running tests
    > make -C tests check
    > # running benchmarks
    > make -C benchmarks GOOGLE_BENCHMARK_PREFIX=<GOOGLE-BENCHMARK_INSTALL_DIR>

Using CMake to generate a static configuration build system:

.. code:: sh

    > mkdir build
    > cd build
    > cmake -D CMAKE_BUILD_TYPE=Release \
            -D frozen.benchmark=ON \
	    -G <"Unix Makefiles" or "Ninja"> ..
    > # building the tests and benchmarks...
    > make                               # ... with make
    > ninja                              # ... with ninja
    > cmake --build .                    # ... with cmake
    > # running the tests...
    > make test                          # ... with make
    > ninja test                         # ... with ninja
    > cmake --build . --target test      # ... with cmake
    > ctest                              # ... with ctest
    > # running the benchmarks...
    > make benchmark                     # ... with make
    > ninja benchmark                    # ... with ninja
    > cmake --build . --target benchmark # ... with cmake

Using CMake to generate an IDE build system with test and benchmark targets

.. code:: sh

    > mkdir build
    > cd build
    > cmake -D frozen.benchmark=ON -G <"Xcode" or "Visual Studio 15 2017"> ..
    > # using cmake to drive the IDE build, test, and benchmark
    > cmake --build . --config Release
    > cmake --build . --target test
    > cmake --build . --target benchmark


Credits
-------

The perfect hashing is strongly inspired by the blog post `Throw away the keys:
Easy, Minimal Perfect Hashing <http://stevehanov.ca/blog/index.php?id=119>`_.

Thanks a lot to Jérôme Dumesnil for his high-quality reviews, and to Chris Beck
for his contributions on perfect hashing.

Contact
-------

Serge sans Paille ``<serge.guelton@telecom-bretagne.eu>``

