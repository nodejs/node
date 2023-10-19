let error = null;
try {
  throw new Error('Caught');
} catch (e) {
  error = e;
}

if (error) {
  throw new Error('Uncaught');
}
