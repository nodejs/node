enum ATrue {
  IsTrue = 1,
  IsFalse = 0
}
 
if (false) {
  console.info('unreachable')
} else if (true) {
  throw Error('throw early')
} else {
  console.info('unreachable')
}
