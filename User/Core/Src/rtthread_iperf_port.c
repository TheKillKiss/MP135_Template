#include "rtconfig.h"

#if defined(RT_USING_LWIP) && defined(RT_LWIP_USING_IPERF)

#include <finsh.h>
#include <lwip/apps/lwiperf.h>
#include <lwip/err.h>
#include <lwip/ip_addr.h>
#include <lwip/tcpip.h>
#include <rtthread.h>
#include <stdlib.h>
#include <string.h>

typedef enum
{
    IPERF_CMD_START_SERVER = 0,
    IPERF_CMD_START_CLIENT,
    IPERF_CMD_STOP
} iperf_cmd_type_t;

typedef struct
{
    iperf_cmd_type_t type;
    ip_addr_t remote_addr;
    rt_uint16_t port;
} iperf_cmd_ctx_t;

static void *iperf_server_session = RT_NULL;
static void *iperf_client_session = RT_NULL;
static const char iperf_role_server;
static const char iperf_role_client;

static void iperf_usage(void)
{
    rt_kprintf("Usage:\n");
    rt_kprintf("  iperf -s [port]          start TCP server, default 5001\n");
    rt_kprintf("  iperf -c <ip> [port]     start TCP client\n");
    rt_kprintf("  iperf stop               stop current iperf sessions\n");
}

static const char *iperf_report_type_name(enum lwiperf_report_type report_type)
{
    switch (report_type)
    {
    case LWIPERF_TCP_DONE_SERVER:
        return "tcp server done";
    case LWIPERF_TCP_DONE_CLIENT:
        return "tcp client done";
    case LWIPERF_TCP_ABORTED_LOCAL:
        return "tcp aborted local";
    case LWIPERF_TCP_ABORTED_LOCAL_DATAERROR:
        return "tcp data error";
    case LWIPERF_TCP_ABORTED_LOCAL_TXERROR:
        return "tcp tx error";
    case LWIPERF_TCP_ABORTED_REMOTE:
        return "tcp aborted remote";
    default:
        return "unknown";
    }
}

static void iperf_report(void *arg,
                         enum lwiperf_report_type report_type,
                         const ip_addr_t *local_addr,
                         rt_uint16_t local_port,
                         const ip_addr_t *remote_addr,
                         rt_uint16_t remote_port,
                         rt_uint32_t bytes_transferred,
                         rt_uint32_t ms_duration,
                         rt_uint32_t bandwidth_kbitpsec)
{
    char local_ip[IPADDR_STRLEN_MAX];
    char remote_ip[IPADDR_STRLEN_MAX];

    (void)ipaddr_ntoa_r(local_addr, local_ip, sizeof(local_ip));
    (void)ipaddr_ntoa_r(remote_addr, remote_ip, sizeof(remote_ip));

    rt_kprintf("[iperf] %s, %s:%u <-> %s:%u, %lu bytes, %lu ms, %lu Kbits/sec\n",
               iperf_report_type_name(report_type),
               local_ip,
               (unsigned int)local_port,
               remote_ip,
               (unsigned int)remote_port,
               (unsigned long)bytes_transferred,
               (unsigned long)ms_duration,
               (unsigned long)bandwidth_kbitpsec);

    if (arg == &iperf_role_client)
    {
        iperf_client_session = RT_NULL;
    }
}

static void iperf_tcpip_start_server(void *parameter)
{
    iperf_cmd_ctx_t *ctx = (iperf_cmd_ctx_t *)parameter;
    ip_addr_t any_addr;

    if (iperf_server_session != RT_NULL)
    {
        rt_kprintf("[iperf] server already started\n");
        rt_free(ctx);
        return;
    }

    ip_addr_set_any(IPADDR_TYPE_V4, &any_addr);
    iperf_server_session = lwiperf_start_tcp_server(&any_addr,
                                                    ctx->port,
                                                    iperf_report,
                                                    (void *)&iperf_role_server);
    if (iperf_server_session == RT_NULL)
    {
        rt_kprintf("[iperf] start server failed, port %u\n", (unsigned int)ctx->port);
    }
    else
    {
        rt_kprintf("[iperf] server listening on port %u\n", (unsigned int)ctx->port);
    }

    rt_free(ctx);
}

static void iperf_tcpip_start_client(void *parameter)
{
    iperf_cmd_ctx_t *ctx = (iperf_cmd_ctx_t *)parameter;
    char remote_ip[IPADDR_STRLEN_MAX];

    if (iperf_client_session != RT_NULL)
    {
        rt_kprintf("[iperf] client already running\n");
        rt_free(ctx);
        return;
    }

    (void)ipaddr_ntoa_r(&ctx->remote_addr, remote_ip, sizeof(remote_ip));
    iperf_client_session = lwiperf_start_tcp_client(&ctx->remote_addr,
                                                    ctx->port,
                                                    LWIPERF_CLIENT,
                                                    iperf_report,
                                                    (void *)&iperf_role_client);
    if (iperf_client_session == RT_NULL)
    {
        rt_kprintf("[iperf] start client failed, remote %s:%u\n",
                   remote_ip,
                   (unsigned int)ctx->port);
    }
    else
    {
        rt_kprintf("[iperf] client connecting to %s:%u\n",
                   remote_ip,
                   (unsigned int)ctx->port);
    }

    rt_free(ctx);
}

static void iperf_tcpip_stop(void *parameter)
{
    iperf_cmd_ctx_t *ctx = (iperf_cmd_ctx_t *)parameter;

    RT_UNUSED(ctx);

    if (iperf_client_session != RT_NULL)
    {
        lwiperf_abort(iperf_client_session);
        iperf_client_session = RT_NULL;
        rt_kprintf("[iperf] client stopped\n");
    }

    if (iperf_server_session != RT_NULL)
    {
        lwiperf_abort(iperf_server_session);
        iperf_server_session = RT_NULL;
        rt_kprintf("[iperf] server stopped\n");
    }

    rt_free(ctx);
}

static int iperf_post_to_tcpip(iperf_cmd_ctx_t *ctx)
{
    err_t err;
    tcpip_callback_fn callback;

    switch (ctx->type)
    {
    case IPERF_CMD_START_SERVER:
        callback = iperf_tcpip_start_server;
        break;
    case IPERF_CMD_START_CLIENT:
        callback = iperf_tcpip_start_client;
        break;
    case IPERF_CMD_STOP:
        callback = iperf_tcpip_stop;
        break;
    default:
        rt_free(ctx);
        return -RT_EINVAL;
    }

    err = tcpip_callback(callback, ctx);
    if (err != ERR_OK)
    {
        rt_kprintf("[iperf] post tcpip callback failed: %d\n", err);
        rt_free(ctx);
        return -RT_ERROR;
    }

    return RT_EOK;
}

static rt_uint16_t iperf_parse_port(const char *text)
{
    int port;

    if (text == RT_NULL)
    {
        return LWIPERF_TCP_PORT_DEFAULT;
    }

    port = atoi(text);
    if ((port <= 0) || (port > 65535))
    {
        return 0;
    }

    return (rt_uint16_t)port;
}

static int cmd_iperf(int argc, char **argv)
{
    iperf_cmd_ctx_t *ctx;
    rt_uint16_t port;

    if (argc < 2)
    {
        iperf_usage();
        return 0;
    }

    ctx = (iperf_cmd_ctx_t *)rt_calloc(1, sizeof(*ctx));
    if (ctx == RT_NULL)
    {
        rt_kprintf("[iperf] no memory\n");
        return -RT_ENOMEM;
    }

    if (strcmp(argv[1], "-s") == 0)
    {
        port = (argc >= 3) ? iperf_parse_port(argv[2]) : LWIPERF_TCP_PORT_DEFAULT;
        if (port == 0U)
        {
            rt_free(ctx);
            iperf_usage();
            return -RT_EINVAL;
        }

        ctx->type = IPERF_CMD_START_SERVER;
        ctx->port = port;
        return iperf_post_to_tcpip(ctx);
    }

    if (strcmp(argv[1], "-c") == 0)
    {
        if ((argc < 3) || (ipaddr_aton(argv[2], &ctx->remote_addr) == 0))
        {
            rt_free(ctx);
            iperf_usage();
            return -RT_EINVAL;
        }

        port = (argc >= 4) ? iperf_parse_port(argv[3]) : LWIPERF_TCP_PORT_DEFAULT;
        if (port == 0U)
        {
            rt_free(ctx);
            iperf_usage();
            return -RT_EINVAL;
        }

        ctx->type = IPERF_CMD_START_CLIENT;
        ctx->port = port;
        return iperf_post_to_tcpip(ctx);
    }

    if (strcmp(argv[1], "stop") == 0)
    {
        ctx->type = IPERF_CMD_STOP;
        return iperf_post_to_tcpip(ctx);
    }

    rt_free(ctx);
    iperf_usage();
    return -RT_EINVAL;
}
MSH_CMD_EXPORT_ALIAS(cmd_iperf, iperf, lwIP iperf2 compatible TCP test);

#endif
