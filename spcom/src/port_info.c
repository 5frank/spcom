
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>

#include <libserialport.h>

#include "assert.h"
#include "common.h"
#include "log.h"

// TODO use async shell print!?
#define PINDENT(N, FMT, ...) printf("%*c" FMT "\n", N, ' ', ##__VA_ARGS__)
#define PTOPIC(FMT, ...) printf(FMT "\n", ##__VA_ARGS__)

struct port_info_s {
    char *devname;
    char *phyinfo;
};

static const char *_matchlist[32];

static int _qsort_strcmp(const void* a, const void* b)
{
    return strcmp(*(const char**)a, *(const char**)b);
}

static int _qsort_port_info_s(const void* a, const void* b)
{
    const struct port_info_s *ainfo = a;
    const struct port_info_s *binfo = b;

    return strcmp(ainfo->devname, binfo->devname);
}

const char *_parity_tostr(enum sp_parity parity)
{
    switch (parity) {
        case SP_PARITY_NONE:
            return "none";
        case SP_PARITY_ODD:
            return "odd";
        case SP_PARITY_EVEN:
            return "even";
        case SP_PARITY_MARK:
            return "mark";
        case SP_PARITY_SPACE:
            return "space";
        default:
        case SP_PARITY_INVALID:
            return "<unknown>";
    }
}

/**
 * note this will briefly open the port in read mode and could alter the
 * state(s) of RTS, CTS, DTR and/or DSR depending on OS and configuration.
 */
int port_info_print_osconfig(const char *name)
{
    int err = 0;
    struct sp_port *port = NULL;
    struct sp_port_config *conf = NULL;
    err = sp_get_port_by_name(name, &port);
    if (err) {
        // TODO print user friendly error here
        LOG_SP_ERR(err, "sp_get_port_by_name");
        goto cleanup;
    }

    err = sp_new_config(&conf);
    assert(!err);

    err = sp_open(port, SP_MODE_READ);
    if (err) {
        LOG_SP_ERR(err, "sp_open");
        goto cleanup;
    }

    // get os defualt. must be _after_ open
    err = sp_get_config(port, conf);
    if (err) {
        LOG_SP_ERR(err, "sp_get_config");
        goto cleanup;
    }

    // closing before using config getters should be ok. 
    err = sp_close(port);
    if (err) {
        LOG_SP_ERR(err, "sp_close");
        // dont cleanup yet. we have config
    }

    int baudrate = -1;
    sp_get_config_baudrate(conf, &baudrate);
    PINDENT(4, "Baudrate: %d", baudrate);

    int databits = -1;
    sp_get_config_bits(conf, &databits);
    PINDENT(4, "Databits: %d", databits);

    int stopbits = -1;
    sp_get_config_stopbits(conf, &stopbits);
    PINDENT(4, "Stopbits: %d", stopbits);

    enum sp_parity parity = -1;
    sp_get_config_parity(conf, &parity);
    PINDENT(4, "Parity:   %s", _parity_tostr(parity));

cleanup:

    if (err)
        PINDENT(4, "<unknown>");

    if (port)
        sp_free_port(port);

    if (conf)
        sp_free_config(conf);

    return err;
}

#if __linux__
static int port_info_add_phyinfo(struct port_info_s *infos, size_t len)
{

#define BASE_PATH       "/dev/serial/by-path/"
/// size of typical filename and some margin
#define FNAME_LEN_MAX   (sizeof("pci-0000:00:14.0-usb-0:4:1.3") + 16)

    int err = 0;
    char buf[sizeof(BASE_PATH) +  FNAME_LEN_MAX] = BASE_PATH;

    DIR *dp = opendir(BASE_PATH);
    if (!dp) {
        LOG_ERR("failed to open %s", BASE_PATH);
        return -1;
    }

    while (1) {
        struct dirent *de = readdir(dp);
        if (!de)
            break;

        if (!de->d_name)
            break;

        if (de->d_name[0] == '.')
            continue;

        size_t slen = strlen(de->d_name);
        if (slen >= (FNAME_LEN_MAX - 1)) {
            fprintf(stderr, "%zu path length", slen);
            continue;
        }

        strcpy(buf + sizeof(BASE_PATH) - 1, de->d_name);
        char *abspath = buf;

        char *devpath = realpath(abspath, NULL);

        // find where this info belongs
        bool found = false;
        for (int i = 0; i < len; i++) {
            struct port_info_s *info = &infos[i];

            assert(info->devname);
            if (strcmp(info->devname, devpath))
                continue;

            if (info->phyinfo) {
                LOG_WRN("%s duplicate phyinfo %s", devpath, de->d_name);
                continue;
            }

            info->phyinfo = strdup(de->d_name);
            assert(info->phyinfo);
            found = true;
            break;
        }

        if (!found) {
            LOG_WRN("%s in %s not listed by libsp", devpath, BASE_PATH);
        }

        free(devpath);
    }

    (void)closedir(dp);

#undef BASE_PATH
#undef FNAME_LEN_MAX

    return err;
}
#else

static int port_info_add_phyinfo(struct port_info_s *infos, size_t len)
{
    return 0;
}
#endif

int port_info_print(const char *name, int verbose)
{
    // TODO use shell printf
    // sp_get_port_name(port);
    struct sp_port *port = NULL;
    int err = sp_get_port_by_name(name, &port);
    if (err) {
        LOG_SP_ERR(err, "sp_get_port_by_name");
        goto cleanup;
    }

    enum sp_transport transport = sp_get_port_transport(port);
    const char *descr = sp_get_port_description(port);
    const char *type;
    switch (transport) {
        case SP_TRANSPORT_NATIVE:
            type = "Native";
            break;
        case SP_TRANSPORT_USB:
            type = "USB";
            break;
        case SP_TRANSPORT_BLUETOOTH:
            type = "Bluetooth";
            break;
        default:
            type = "Unknown";
            break;
    }

    if (!verbose) {
        PTOPIC("%s: %s: %s", name, type, descr);
        goto cleanup;
    }

    PTOPIC("Device: %s", name);
    PINDENT(2, "Type: %s", type);
    PINDENT(2, "Description: %s", descr);

    if (transport == SP_TRANSPORT_USB) {

        PINDENT(2, "Manufacturer: %s", sp_get_port_usb_manufacturer(port));
        PINDENT(2, "Product: %s", sp_get_port_usb_product(port));
        PINDENT(2, "Serial: %s", sp_get_port_usb_serial(port));

        // USB vendorId and productId
        int usb_vid, usb_pid;
        sp_get_port_usb_vid_pid(port, &usb_vid, &usb_pid);
        PINDENT(2, "VID: %04X PID: %04X", usb_vid, usb_pid);

        // bus and address
        int usb_bus, usb_address;
        sp_get_port_usb_bus_address(port, &usb_bus, &usb_address);
        PINDENT(2, "Bus: %d Address: %d", usb_bus, usb_address);
    }

    else if (transport == SP_TRANSPORT_BLUETOOTH) {
        PINDENT(2, "Type: Bluetooth");
        PINDENT(2, "MAC: %s", sp_get_port_bluetooth_address(port));
    }

cleanup:

    if (port)
        sp_free_port(port);

    return err;
}

static size_t _sp_portlist_len(struct sp_port **portlist)
{
    size_t i = 0;
    while (1) {
        if (!portlist[i])
            break;
        i++;
    }
    return i;
}

int port_info_print_list(int verbose)
{
    struct sp_port **portlist = NULL;
    struct port_info_s *infos = NULL;

    int err = sp_list_ports(&portlist);
    if (err) {
        LOG_SP_ERR(err, "sp_list_ports");
        goto cleanup;
    }

    size_t list_len = _sp_portlist_len(portlist);
    if (!list_len)
        goto cleanup;

    infos = calloc(list_len, sizeof(struct port_info_s));
    assert(infos);

    for (int i = 0; i < list_len; i++) {
        struct sp_port *port = portlist[i];
        assert(port); // should match list_len
        infos[i].devname = sp_get_port_name(port);
        infos[i].phyinfo = NULL;
    }

    qsort(infos, list_len, sizeof(infos[0]), _qsort_port_info_s);

    bool show_phyinfo = verbose > 1;

    if (show_phyinfo) {
        err = port_info_add_phyinfo(infos, list_len);
        if (err)
            goto cleanup;
    }
    // `infos` still holds refrence to `portlist`
    for (int i = 0; i < list_len; i++) {

        struct port_info_s *info = &infos[i];

        err = port_info_print(info->devname, verbose);
        if (err)
            continue;

        if (show_phyinfo && info->phyinfo) {
            PINDENT(2, "Phyinfo: %s", info->phyinfo);
        }

        if (verbose > 2) {
            PINDENT(2, "OS Settings:");
            err = port_info_print_osconfig(info->devname);
            if (err)
                continue;
        }
    }

cleanup:
    if (portlist)
        sp_free_port_list(portlist);

    if (infos)
        free(infos);

    return err;
}

const char** port_info_complete(const char *s)
{
    _matchlist[0] = NULL;
    struct sp_port **portlist = NULL;
    int err = sp_list_ports(&portlist);
    if (err) {
        LOG_SP_ERR(err, "sp_list_ports");
        goto cleanup;
    }
    int slen = s ? strlen(s) : 0;
    int n = 0;
    // -1 to save space for last NULL
    for (n = 0; (ARRAY_LEN(_matchlist) - 1); n++) {
        struct sp_port *port = portlist[n];
        if (!port)
            break;

        const char *name = sp_get_port_name(port);
        if (!name)
            continue;

        if (s && strncmp(name, s, slen))
            continue;

        _matchlist[n] = strdup(name);
    }

    _matchlist[n] = NULL;

    // TODO need to sort these or fixed by bash/readline?
    const bool sort = false;

    // dont pass NULL to qsort
    if (n > 0 && sort)
        qsort(_matchlist, n - 1, sizeof(_matchlist[0]), _qsort_strcmp);

cleanup:
    if (portlist)
        sp_free_port_list(portlist);

    return _matchlist;
}

