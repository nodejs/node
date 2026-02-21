setup({ single_test: true });
setTimeout(done, Math.pow(2, 32));
setTimeout(assert_unreached, 100);
