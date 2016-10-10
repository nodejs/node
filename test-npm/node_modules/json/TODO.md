- `make check` add jsl


# someday/maybe

- get some perf numbers (timeit-based. 'Ben' isn't cutting it)

- pjson-like coloring of JSON output: http://igorgue.com/pjson/

- fix this in man page (ronn fix?):

        Index: /Users/trentm/tm/json/docs/json.1
        index 45f625f..7230bc5 100644
        --- a/docs/json.1
        +++ b/docs/json.1
        @@ -407,7 +407,7 @@ $ curl \-s http://github\.com/api/v2/json/repos/search/nodejs | json repositorie
             "created_at": "2009/06/26 11:56:01 \-0700",
             "pushed": "2011/09/28 10:27:26 \-0700",
             "forks": 345,
        -\.\.\.
        +\&\.\.\.
         .
         .fi
         .

  Tips from mdocml.

- Support slice indexing into an array:
      $ echo '["a","b","c"]' | jsondev '[1:]'
      ["b", "c"]

