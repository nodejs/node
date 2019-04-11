# flake8: noqa

macro DCHECK(x) = do { if (!(x)) (process._rawDebug("DCHECK: x == true"), process.abort()) } while (0);
macro DCHECK_EQ(a, b) = DCHECK((a) === (b));
macro DCHECK_GE(a, b) = DCHECK((a) >= (b));
macro DCHECK_GT(a, b) = DCHECK((a) > (b));
macro DCHECK_LE(a, b) = DCHECK((a) <= (b));
macro DCHECK_LT(a, b) = DCHECK((a) < (b));
macro DCHECK_NE(a, b) = DCHECK((a) !== (b));
