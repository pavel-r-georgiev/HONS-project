
#include <czmq.h>
#define PING_PORT_NUMBER 1500
#define PING_MSG_SIZE    1
#define PING_INTERVAL    1000  //  Once per second

volatile sig_atomic_t stop_monitoring_loop;

void stopMonitoringLoop(int signum) {
    stop_monitoring_loop = 1;
}

void monitorTraffic(zactor_t *listener){
    signal(SIGINT, stopMonitoringLoop);

    while (!stop_monitoring_loop) {
        char *ipaddress, *received;
        zstr_recvx (listener, &ipaddress, &received, NULL);
        printf("IP Address: %s, Data: %s \n", ipaddress, received);
        zstr_free (&ipaddress);
        zstr_free (&received);
    }
}

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
    // We will listen to anything (empty subscription)
    zsock_send (listener, "sb", "SUBSCRIBE", "", 0);

    char address[10];
    char action[10];

    while(!zctx_interrupted){
        printf("Enter command: ");
        scanf ("%[^\n]%*c", action);

        if(strcmp(action, "ABORT") == 0){
            break;
        }

        if(strcmp(action, "TOGGLE") == 0){
            printf("Enter address: ");
            scanf ("%[^\n]%*c", address);
            printf("Silencing: %s", address);
            zsock_send (speaker, "sbi", "PUBLISH", address, sizeof(address) + 2);
            zstr_sendx (speaker, "SILENCE", NULL);
        }

        if(streq(action, "MONITOR")) {
            stop_monitoring_loop = 0;
            monitorTraffic(listener);
        }
    }

    zactor_destroy (&listener);
    zactor_destroy (&speaker);
    printf("Destroyed");
}

