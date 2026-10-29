try {
  require('./tla.mjs');
} catch (e) {
  console.log('e.code === ERR_REQUIRE_ASYNC_MODULE', e.code === 'ERR_REQUIRE_ASYNC_MODULE');
}

try {
  require('./tla.mjs');
} catch (e) {
  console.log('e.code === ERR_REQUIRE_ASYNC_MODULE', e.code === 'ERR_REQUIRE_ASYNC_MODULE');
}
