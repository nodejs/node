# lru cache

A cache object that deletes the least-recently-used items.

Usage:

    var LRU = require("lru-cache")
      , cache = LRU(10, // max length. default = Infinity

                    // calculate how "big" each item is
                    //
                    // defaults to function(){return 1}, ie, just limit
                    // the item count, without any knowledge as to their
                    // relative size.
                    function (item) { return item.length },

                    // maxAge in ms
                    // defaults to infinite
                    // items are not pre-emptively pruned, but they
                    // are deleted when fetched if they're too old.
                    1000 * 60)

    cache.set("key", "value")
    cache.get("key") // "value"

    cache.reset()    // empty the cache

If you put more stuff in it, then items will fall out.

If you try to put an oversized thing in it, then it'll fall out right
away.

RTFS for more info.
