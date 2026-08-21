[
  {
    "input": "z=b&a=b&z=a&a=a",
    "output": [["a", "b"], ["a", "a"], ["z", "b"], ["z", "a"]]
  },
  {
    "input": "\uFFFD=x&\uFFFC&\uFFFD=a",
    "output": [["\uFFFC", ""], ["\uFFFD", "x"], ["\uFFFD", "a"]]
  },
  {
    "input": "ﬃ&🌈", // 🌈 > code point, but < code unit because two code units
    "output": [["🌈", ""], ["ﬃ", ""]]
  },
  {
    "input": "é&e\uFFFD&e\u0301",
    "output": [["e\u0301", ""], ["e\uFFFD", ""], ["é", ""]]
  },
  {
    "input": "z=z&a=a&z=y&a=b&z=x&a=c&z=w&a=d&z=v&a=e&z=u&a=f&z=t&a=g",
    "output": [["a", "a"], ["a", "b"], ["a", "c"], ["a", "d"], ["a", "e"], ["a", "f"], ["a", "g"], ["z", "z"], ["z", "y"], ["z", "x"], ["z", "w"], ["z", "v"], ["z", "u"], ["z", "t"]]
  },
  {
    "input": "bbb&bb&aaa&aa=x&aa=y",
    "output": [["aa", "x"], ["aa", "y"], ["aaa", ""], ["bb", ""], ["bbb", ""]]
  },
  {
    "input": "z=z&=f&=t&=x",
    "output": [["", "f"], ["", "t"], ["", "x"], ["z", "z"]]
  },
  {
    "input": "a🌈&a💩",
    "output": [["a🌈", ""], ["a💩", ""]]
  }
].forEach((val) => {
  test(() => {
    let params = new URLSearchParams(val.input),
        i = 0
    params.sort()
    for(let param of params) {
      assert_array_equals(param, val.output[i])
      i++
    }
  }, "Parse and sort: " + val.input)

  test(() => {
    let url = new URL("?" + val.input, "https://example/")
    url.searchParams.sort()
    let params = new URLSearchParams(url.search),
        i = 0
    for(let param of params) {
      assert_array_equals(param, val.output[i])
      i++
    }
  }, "URL parse and sort: " + val.input)
})

test(function() {
  const url = new URL("http://example.com/?")
  url.searchParams.sort()
  assert_equals(url.href, "http://example.com/")
  assert_equals(url.search, "")
}, "Sorting non-existent params removes ? from URL")
