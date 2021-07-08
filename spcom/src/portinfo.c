
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <libserialport.h>

#include "assert.h"
#include "log.h"
#include "utils.h"

// TODO use async shell print!?
#define PINDENT(N, FMT, ...) printf("%*c" FMT "\n", N, ' ', ##__VA_ARGS__)
#define PTOPIC(FMT, ...) printf(FMT "\n", ##__VA_ARGS__)

static const char *_devnames[64];
static const char *_matchlist[32];

static int _qsort_strcmp(const void* a, const void* b)
{
  return strcmp(*(const char**)a, *(const char**)b);
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
int portinfo_print_osconfig(const char *name)
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

int portinfo_print(const char *name, int verbose)
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
    const char *descr= sp_get_port_description(port);
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


int portinfo_print_list(int verbose)
{
    struct sp_port **portlist = NULL;

    int err = sp_list_ports(&portlist);
    if (err) {
        LOG_SP_ERR(err, "sp_list_ports");
        goto cleanup;
    }

    int n = 0;
    for (n = 0; ARRAY_LEN(_devnames); n++) {
        struct sp_port *port = portlist[n];
        if (!port)
            break;
        _devnames[n] = sp_get_port_name(port);
    }

    if (n >= ARRAY_LEN(_devnames)) {
        LOG_DBG("port list truncated to %d", (int)ARRAY_LEN(_devnames));
    }

    qsort(_devnames, n, sizeof(_devnames[0]), _qsort_strcmp);

    // `_devnames` still holds refrence to `portlist`
    for (int i = 0; i < n; i++) {
        // autocomplete format
        if (verbose < 0) {
            printf("%s ", _devnames[i]);
            continue;
        }

        err = portinfo_print(_devnames[i], verbose);
        if (err)
            continue;

        if (verbose < 2) 
            continue;

        PINDENT(2, "OS Settings:");
        err = portinfo_print_osconfig(_devnames[i]);
        if (err)
            continue;
    }

cleanup:
    if (portlist)
        sp_free_port_list(portlist);

    return err;
}

const char** portinfo_match(const char *s)
{
    // TODO need to sort these or fixed by bash/readline?
    const bool sort = false;

    _matchlist[0] = NULL;
    struct sp_port **portlist = NULL;
    int err = sp_list_ports(&portlist);
    if (err) {
        LOG_SP_ERR(err, "sp_list_ports");
        goto cleanup;
    }
    int slen = strlen(s);
    int n = 0;
    // -1 to save space for last NULL
    for (n = 0; (ARRAY_LEN(_matchlist) - 1); n++) {
        struct sp_port *port = portlist[n];
        if (!port)
            break;

        const char *name = sp_get_port_name(port);
        if (!name)
            continue;

        if (strncmp(name, s, slen))
            continue;

        _matchlist[n] = strdup(name);
    }

    _matchlist[n] = NULL;

    // dont pass NULL to qsort
    if (n > 0 && sort)
        qsort(_matchlist, n - 1, sizeof(_matchlist[0]), _qsort_strcmp);

cleanup:
    if (portlist)
        sp_free_port_list(portlist);

    return _matchlist;
}

