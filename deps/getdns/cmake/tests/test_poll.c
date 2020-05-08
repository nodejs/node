#ifdef HAVE_SYS_POLL_H
#include <sys/poll.h>
#else
#include <poll.h>
#endif

int main (int ac, char *av[])
{
    int rc;
    rc = poll((struct pollfd *)(0), 0, 0);
}
