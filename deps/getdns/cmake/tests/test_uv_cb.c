#include <uv.h>

void test_cb(uv_timer_t *handle)
{
    (void) handle;
}

int main(int ac, char *av[])
{
    uv_timer_cb cb = test_cb;
    (*cb)(0);
}
