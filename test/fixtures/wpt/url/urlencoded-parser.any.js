[
  { "input": "test", "output": [["test", ""]] },
  { "input": "\uFEFFtest=\uFEFF", "output": [["\uFEFFtest", "\uFEFF"]] },
  { "input": "%EF%BB%BFtest=%EF%BB%BF", "output": [["\uFEFFtest", "\uFEFF"]] },
  { "input": "%FE%FF", "output": [["\uFFFD\uFFFD", ""]] },
  { "input": "%FF%FE", "output": [["\uFFFD\uFFFD", ""]] },
  { "input": "†&†=x", "output": [["†", ""], ["†", "x"]] },
  { "input": "%C2", "output": [["\uFFFD", ""]] },
  { "input": "%C2x", "output": [["\uFFFDx", ""]] },
  { "input": "_charset_=windows-1252&test=%C2x", "output": [["_charset_", "windows-1252"], ["test", "\uFFFDx"]] },
  { "input": '', "output": [] },
  { "input": 'a', "output": [['a', '']] },
  { "input": 'a=b', "output": [['a', 'b']] },
  { "input": 'a=', "output": [['a', '']] },
  { "input": '=b', "output": [['', 'b']] },
  { "input": '&', "output": [] },
  { "input": '&a', "output": [['a', '']] },
  { "input": 'a&', "output": [['a', '']] },
  { "input": 'a&a', "output": [['a', ''], ['a', '']] },
  { "input": 'a&b&c', "output": [['a', ''], ['b', ''], ['c', '']] },
  { "input": 'a=b&c=d', "output": [['a', 'b'], ['c', 'd']] },
  { "input": 'a=b&c=d&', "output": [['a', 'b'], ['c', 'd']] },
  { "input": '&&&a=b&&&&c=d&', "output": [['a', 'b'], ['c', 'd']] },
  { "input": 'a=a&a=b&a=c', "output": [['a', 'a'], ['a', 'b'], ['a', 'c']] },
  { "input": 'a==a', "output": [['a', '=a']] },
  { "input": 'a=a+b+c+d', "output": [['a', 'a b c d']] },
  { "input": '%=a', "output": [['%', 'a']] },
  { "input": '%a=a', "output": [['%a', 'a']] },
  { "input": '%a_=a', "output": [['%a_', 'a']] },
  { "input": '%61=a', "output": [['a', 'a']] },
  { "input": '%61+%4d%4D=', "output": [['a MM', '']] },
  { "input": "id=0&value=%", "output": [['id', '0'], ['value', '%']] },
  { "input": "b=%2sf%2a", "output": [['b', '%2sf*']]},
  { "input": "b=%2%2af%2a", "output": [['b', '%2*f*']]},
  { "input": "b=%%2a", "output": [['b', '%*']]}
].forEach((val) => {
  test(() => {
    let sp = new URLSearchParams(val.input),
        i = 0
    for (let item of sp) {
       assert_array_equals(item, val.output[i])
       i++
    }
  }, "URLSearchParams constructed with: " + val.input)

  promise_test(() => {
    let init = new Request("about:blank", { body: val.input, method: "LADIDA", headers: {"Content-Type": "application/x-www-form-urlencoded;charset=windows-1252"} }).formData()
    return init.then((fd) => {
      let i = 0
      for (let item of fd) {
         assert_array_equals(item, val.output[i])
         i++
      }
    })
  }, "request.formData() with input: " + val.input)

  promise_test(() => {
    let init = new Response(val.input, { headers: {"Content-Type": "application/x-www-form-urlencoded;charset=shift_jis"} }).formData()
    return init.then((fd) => {
      let i = 0
      for (let item of fd) {
         assert_array_equals(item, val.output[i])
         i++
      }
    })
  }, "response.formData() with input: " + val.input)
});
