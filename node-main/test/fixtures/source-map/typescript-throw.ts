enum ATrue {
  IsTrue = 1,
  IsFalse = 0
}

if (false) {
  console.info('unreachable')
} else if (true) {
  console.info('reachable')
} else {
  console.info('unreachable')
}

function branch (a: ATrue) {
  if (a === ATrue.IsFalse) {
    console.info('a = false')
  } else if (a === ATrue.IsTrue) {
    throw Error('an exception');
  } else {
    console.info('a = ???')
  }
}

branch(ATrue.IsTrue)
