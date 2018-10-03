
#include <czmq.h>
#define PING_PORT_NUMBER 1500
#define PING_MSG_SIZE    1
#define PING_INTERVAL    1000  //  Once per second

int main(void)
{
    //  Create new beacon
    zactor_t *speaker = zactor_new (zbeacon, NULL);
    zactor_t *listener = zactor_new (zbeacon, NULL);

    //  Enable verbose logging of commands and activity:
    zstr_sendx (speaker, "VERBOSE", NULL);
    zstr_sendx (listener, "VERBOSE", NULL);

    zsock_send (speaker, "si", "CONFIGURE", PING_PORT_NUMBER);
    zsock_send (listener, "si", "CONFIGURE", PING_PORT_NUMBER);


//    zstr_sendx (speaker, "PUBLISH", "STOP", "1000", NULL);
    zsock_send (speaker, "sbi", "PUBLISH", "!", PING_MSG_SIZE, PING_INTERVAL);
    // We will listen to anything (empty subscription)
    zsock_send (listener, "sb", "SUBSCRIBE", "", 0);

    while (!zctx_interrupted) {
//        zframe_t *content = zframe_recv (listener);
        char *ipaddress, *received;
        zstr_recvx (listener, &ipaddress, &received, NULL);
        printf("IP Address: %s, Data: %s \n", ipaddress, received);
        zstr_free (&ipaddress);
        zstr_free (&received);
    }

    // Wait for at most 1/2 second if there's no broadcasting
    zsock_set_rcvtimeo (listener, 5*PING_INTERVAL);

    zactor_destroy (&listener);
    zactor_destroy (&speaker);
}