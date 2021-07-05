async function* toUpper() {
  while (true) {
    yield this.sent.toUpperCase();
  }
}

async function* src() {
  yield 'hello'
  yield 'world'
}

async function* pump(src, gen) {
  const context = { sent: null }
  const dst = gen.call(context)
  const iterator = dst[Symbol.asyncIterator]()
  try {
    for await (const chunk of src) {
      context.sent = chunk
      const { done, value } = await iterator.next()
      if (done) {
        return
      }
      yield value
    }
  } finally {
    iterator.return()
  }
}

for await (const chunk of pump(src(), toUpper)) {
  console.log(chunk)
}
