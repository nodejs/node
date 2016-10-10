Test handling of a grouping adjacent objects into an array:

    $ echo '{"one": 1}{"two": 1}' | json -g
    [
      {
        "one": 1
      },
      {
        "two": 1
      }
    ]

