macro CHECK(x) = do { if (!(x)) (process._rawDebug("CHECK: x == true"), process.abort()) } while (0);
macro CHECK_EQ(a, b) = CHECK((a) === (b));
macro CHECK_GE(a, b) = CHECK((a) >= (b));
macro CHECK_GT(a, b) = CHECK((a) > (b));
macro CHECK_LE(a, b) = CHECK((a) <= (b));
macro CHECK_LT(a, b) = CHECK((a) < (b));
macro CHECK_NE(a, b) = CHECK((a) !== (b));
