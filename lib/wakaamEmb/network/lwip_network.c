#ifdef LWIP


#include "wakaama_network.h"
#include "wakaama_simple_client.h"
#include "network_utils.h"
#include "../wakaama/internals.h"
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>


#ifndef MAX_PACKET_SIZE
#define MAX_PACKET_SIZE 1024
#endif

#include "lwip/api.h"
#include "lwip/sys.h"
#include "lwip/udp.h"
#include "lwip/dns.h"
#include "lwip/timers.h"
#include "../logger.h"

typedef struct udp_pcb udp_pcb_t;

typedef struct _connection_t
{
    struct _connection_t *  next;
    udp_pcb_t *             sock;
    ip_addr_t               addr;
    uint16_t                port;
} connection_t;

connection_t * connection_find(connection_t * connList, const ip_addr_t * addr);
connection_t * connection_create(network_t *network, char * host, char * port);
void connection_free(connection_t * connList);
void udp_raw_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port);

uint8_t lwm2m_network_init(lwm2m_context_t * contextP, const char* localPort)
{
    //Serial.println("WiFi connected");
    //lwm2m_log("lwm2m_network_init \n");
    // The network can only be initialized once. We also need the userdata pointer
    // and therefore check if it is not used so far.
    if (contextP->userData != NULL)
    {
        //lwm2m_log("No Context \n");
        return 0;
    }

    //lwm2m_log("Allocate memory for the network structure \n");
    // Allocate memory for the network structure
    contextP->userData = lwm2m_malloc(sizeof(network_t));
    
    if (contextP->userData == NULL)
    {
        //lwm2m_log("Faild Allocate memory for the network structure \n");
        return 0;
    }

    network_t* network = (network_t*)contextP->userData;
    memset(network, 0, sizeof(network_t));

    //lwm2m_log("Check Local port \n");
    if (localPort) {
        // Server
        network->type = NET_SERVER_PROCESS;
        //lwm2m_log("Server \n");
    } else {
        // client
        network->type = NET_CLIENT_PROCESS;
        //lwm2m_log("Client \n");
    }

    network->socket_handle = (int*)udp_new();
    if (network->socket_handle == NULL)
    {
        contextP->userData = 0;
        lwm2m_free(network);
        return 0;
    }
    //lwm2m_log("bind bind \n");
    udp_bind((udp_pcb_t*)network->socket_handle, IP_ADDR_ANY, localPort==NULL ? 12873 : atoi(localPort));
    //lwm2m_log("bind recb \n");
    udp_recv((udp_pcb_t*)network->socket_handle, (udp_recv_fn)udp_raw_recv, contextP);

    network->open_listen_sockets = 1;

    //lwm2m_log("Return \n");
    return network->open_listen_sockets;
}

bool __attribute__((weak)) lwm2m_network_process(lwm2m_context_t * contextP) {
    // NOOP for lwip, because udp_recv() registers a callback for incoming packets
    // and they are processed in udp_raw_recv()
    return true;
}

void udp_raw_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    lwm2m_context_t * contextP = (lwm2m_context_t*)arg;
    if(p == NULL)
        return;

    network_t* network = (network_t*)contextP->userData;

    connection_t * connP = connection_find((connection_t*)network->connection_list, addr);

    if (connP == NULL && network->type == NET_SERVER_PROCESS) {
        connP = (connection_t *)malloc(sizeof(connection_t));
        if (connP != NULL)
        {
            //lwm2m_log("Server: Add client connection\n");
            connP->sock = (udp_pcb_t*)network->socket_handle;
            connP->port = port;
            memcpy(&(connP->addr), addr, sizeof(ip_addr_t));
            connP->next = (struct _connection_t *)network->connection_list;
            network->connection_list = (struct _connection_t_*)connP;
        }
    }
    digitalWrite(5,1);
    if (connP != NULL) {
        #if (LWIP_IPV4 && LWIP_IPV6)
        //lwm2m_log("Receiving %d bytes from [%s]:%hu\r\n",
                    p->tot_len,
                    connP->addr.type==IPADDR_TYPE_V4 ?
                    ip4addr_ntoa(&connP->addr.u_addr.ip4) :
                    ip6addr_ntoa(&connP->addr.u_addr.ip6),
                    connP->port);
        #endif
        #ifdef LWIP_IPV4
        //lwm2m_log("Receiving %d bytes from [%s]:%hu\r\n",
                    p->tot_len,
                    ip4addr_ntoa(&connP->addr.u_addr.ip4)
                    connP->port);
        #endif

        lwm2m_log("Receiving ");
        lwm2m_log_int(p->tot_len);
            lwm2m_log(p->payload);
            lwm2m_log_int(p->tot_len);
        lwm2m_handle_packet(contextP, p->payload, p->tot_len, connP);
        lwm2m_log("done lwm2m_handle_packet ");
    } else {
        //lwm2m_log("received bytes ignored!\r\n");
    }
    digitalWrite(5,0);

    pbuf_free(p);
}

bool lwm2m_network_client_process(lwm2m_context_t * contextP) {
    sys_check_timeouts();
    return true;
}

void lwm2m_network_close(lwm2m_context_t * contextP) {
    network_t* network = (network_t*)contextP->userData;
    if (!network) return;

    if (network->socket_handle) {
        udp_remove((udp_pcb_t*)network->socket_handle);
    }

    network->socket_handle = NULL;
    network->open_listen_sockets = 0;
}

uint8_t __attribute__((weak)) lwm2m_buffer_send(void * sessionH,
                          uint8_t * buffer,
                          size_t length,
                          void * userdata)
{
    //lwm2m_log("start lwm2m_buffer_send");
    connection_t * connP = (connection_t*) sessionH;
    network_t* network = (network_t*)userdata;

    if (connP == NULL)
    {
        ////lwm2m_log("#> failed sending %lu bytes, missing connection\r\n", length);
        //lwm2m_log("#> failed sending %lu bytes, missing connection\r\n");
        return COAP_500_INTERNAL_SERVER_ERROR ;
    }

    #ifdef LWM2M_WITH_LOGS
    const char* a = connP->addr.type==IPADDR_TYPE_V4 ?
                ip4addr_ntoa(&connP->addr.u_addr.ip4) :
                ip6addr_ntoa(&connP->addr.u_addr.ip6);
    const char* b;
    if (network->net_if_out)
    {
        b = connP->addr.type==IPADDR_TYPE_V4 ?
                ip4addr_ntoa(&netif_ip_addr4((struct netif *)network->net_if_out)->u_addr.ip4) :
                ip6addr_ntoa(&netif_ip_addr6((struct netif *)network->net_if_out, 0)->u_addr.ip6);
    }
    else
    {
        b = "N/A";
    }

    //lwm2m_log("Sending %d bytes to [%s]:%hu. Interface IP: %s. Is Server: %i\r\n", length, a,
        connP->port, b, network->type==NET_SERVER_PROCESS);
    #endif

    //struct pbuf * pb = pbuf_alloc(PBUF_TRANSPORT, length, PBUF_REF);
    struct pbuf * pb = pbuf_alloc(PBUF_TRANSPORT, length, PBUF_RAM);
    //pb->payload = buffer;
    memcpy(pb->payload, buffer, length);

    err_t err;
    //lwm2m_log("udp_sendto");
    if (network->net_if_out)
        err = udp_sendto_if(connP->sock, pb, &connP->addr, connP->port, network->net_if_out);
    else
        err = udp_sendto(connP->sock, pb, &connP->addr, connP->port);
    //lwm2m_log("done udp_sendto");
    #if LWIP_VERSION_MAJOR == 2
        //lwm2m_log("LWIP_VERSION_MAJOR 2");
    #endif 
    pbuf_free(pb); //De-allocate packet buffer
    //lwm2m_log("done pbuf_free");

    
    if (err != ERR_OK)
    {
        //lwm2m_log("#> failed sending %lu bytes\r\n");
       // //lwm2m_log(length);
        return COAP_500_INTERNAL_SERVER_ERROR ;
    }
    //lwm2m_log("exit lwm2m_buffer_send");
    return COAP_NO_ERROR;
}

bool lwm2m_session_is_equal(void * session1,
                            void * session2,
                            void * userData)
{
    return (session1 == session2);
}

void * lwm2m_connect_server(uint16_t secObjInstID,
                            void * userData)
{
    char * host;
    char * port;
    char uri[255];

    if (!lwm2m_get_server_uri(secObjInstID, uri, sizeof(uri)))
        return NULL;

    decode_uri(uri, &host, &port);

    ////lwm2m_log("Connecting to %s %s\r\n", host, port);
    //lwm2m_log("Connecting to %s %s\r\n");
    //lwm2m_log(host);
    //lwm2m_log(port);

    network_t* network = (network_t*)userData;
    connection_t * newConnP = connection_create(network, host, port);
    //lwm2m_log("connection_create");

    if (newConnP == NULL) {
        //lwm2m_log("Connection creation failed.\r\n");
    }
    else {
        network->connection_list = (struct _connection_t_*)newConnP;
    }

    //lwm2m_log("lwm2m_connect_server return");
    return (void *)newConnP;
}

void lwm2m_close_connection(void * sessionH,
                            void * userData)
{
    network_t* network = (network_t*)userData;
    connection_free((connection_t*)network->connection_list);
}

connection_t * connection_find(connection_t * connList, const ip_addr_t * addr)
{
    connection_t * connP;

    connP = connList;
    while (connP != NULL)
    {
        if (ip_addr_cmp(&connP->addr, addr))
        {
            return connP;
        }
        connP = connP->next;
    }

    return connP;
}

connection_t * connection_create(network_t* network,
                                 char * host,
                                 char * port)
{
    ip_addr_t addr;
    if (!ipaddr_aton(host, &addr))
    {
        return 0;
    }

    int portNo = atoi(port);
    if (portNo == 0)
        return 0;

    connection_t * connP = (connection_t *)malloc(sizeof(connection_t));
    if (connP != NULL)
    {
        connP->sock = (udp_pcb_t*)network->socket_handle;
        connP->port = portNo;
        memcpy(&(connP->addr), &addr, sizeof(ip_addr_t));
        connP->next = (struct _connection_t *)network->connection_list;
    }

    return connP;
}

void connection_free(connection_t * connList)
{
    while (connList != NULL)
    {
        connection_t * nextP;

        nextP = connList->next;
        free(connList);

        connList = nextP;
    }
}

#endif
