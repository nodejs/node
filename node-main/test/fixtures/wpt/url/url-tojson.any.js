test(() => {
  const a = new URL("https://example.com/")
  assert_equals(JSON.stringify(a), "\"https://example.com/\"")
})
