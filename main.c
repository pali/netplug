#include "netplug.h"


int
main(int argc, char *argv[])
{
    int fd = netlink_open();
    netlink_request_dump(fd);
    netlink_receive_dump(fd, if_info_save_interface, NULL);
    netlink_listen(fd, NULL, NULL);
    return fd ? 0 : 0;
}


/*
 * Local variables:
 * c-file-style: "stroustrup"
 * End:
 */
