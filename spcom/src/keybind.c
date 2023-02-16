#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
// local
#include "assert.h"
#include "btable.h"
#include "common.h"
#include "keybind.h"
#include "log.h"
#include "opt.h"
#include "vt_defs.h"

struct keybind_map {
    // have key sequence if both in ckey_tbl and seq0_tbl
    btable_t ckey_tbl[BTABLE_NBITS_TO_NWORDS(UINT8_MAX)];
    btable_t seq0_tbl[BTABLE_NBITS_TO_NWORDS(UINT8_MAX)];
    btable_t seq1_tbl[BTABLE_NBITS_TO_NWORDS(UINT8_MAX)];
    uint32_t seq_map[16];
    uint16_t map[16];
    uint8_t seq0; // leader ctrl key
};

static struct keybind_map keybind_map;

static int kb_error(int err, const char *msg)
{
    fprintf(stderr, msg);
    return err;
}

#define KB_UNPACK_KEY(VAL) ((VAL)&0xFF)
#define KB_UNPACK_ACTION(VAL) (((VAL) >> 8) & 0xFF)
#define KB_PACK(KEY, ACTION)                                                   \
    (0 | (((uint16_t)(KEY)&0xFF)) | (((uint16_t)(ACTION)&0xFF) << 8))

/// add single key binding action
static int kb_add_action(struct keybind_map *kbm, uint8_t key, uint8_t action)
{
    assert(key);
    if (btable_get(kbm->seq0_tbl, key))
        return kb_error(EEXIST, "conflicting key");

    for (int i = 0; i < ARRAY_LEN(kbm->map); i++) {
        uint16_t map_val = kbm->map[i];
        if (map_val)
            continue;

        kbm->map[i] = KB_PACK(key, action);
        btable_set(kbm->ckey_tbl, key);
        return 0;
    }

    return kb_error(ENOMEM, "max limit key bindings");
}

/// get single key binding action
static uint8_t kb_get_action(const struct keybind_map *kbm, uint8_t key)
{
    for (int i = 0; i < ARRAY_LEN(kbm->map); i++) {
        uint16_t map_val = kbm->map[i];
        if (!map_val)
            break;

        if (KB_UNPACK_KEY(map_val) == key)
            return KB_UNPACK_ACTION(map_val);
    }

    return 0;
}

#undef KB_UNPACK_KEY
#undef KB_UNPACK_ACTION
#undef KB_PACK

#define KB_SEQ_PACK(CTLK, FOLLOWK, ACTION)                                     \
    (0 | (((uint32_t)(CTLK)&0xFF) << 0) | (((uint32_t)(FOLLOWK)&0xFF) << 8)    \
     | (((uint32_t)(ACTION)&0xFF) << 16))
#define KB_SEQ_UNPACK_ID(VAL) ((VAL)&0xFFFF)
#define KB_SEQ_UNPACK_ACTION(VAL) (((VAL) >> 16) & 0xFF)

/// set key sequence action
static int kb_seq_add_action(struct keybind_map *kbm, uint8_t ctlkey,
                             uint8_t followk, uint8_t action)
{
    assert(ctlkey);
    assert(followk);

    if (btable_get(kbm->ckey_tbl, ctlkey))
        return kb_error(EEXIST, "conflicting ctlkey");

    for (int i = 0; i < ARRAY_LEN(kbm->seq_map); i++) {
        uint32_t map_val = kbm->seq_map[i];
        if (map_val)
            continue;

        // have empty slot
        kbm->seq_map[i] = KB_SEQ_PACK(ctlkey, followk, action);
        btable_set(kbm->ckey_tbl, ctlkey);
        btable_set(kbm->seq0_tbl, ctlkey);
        btable_set(kbm->seq1_tbl, followk);

        return 0;
    }

    return kb_error(ENOMEM, "max limit key bindings");
}

/** get key sequence action
 * return 0 if not unmapped
 */
static uint8_t kb_seq_get_action(const struct keybind_map *kbm, uint8_t ctlkey,
                                 uint8_t followk)
{
    uint32_t id = KB_SEQ_PACK(ctlkey, followk, 0);

    for (int i = 0; i < ARRAY_LEN(kbm->seq_map); i++) {
        uint32_t map_val = kbm->seq_map[i];
        if (!map_val)
            break;

        if (KB_SEQ_UNPACK_ID(map_val) == id)
            return KB_SEQ_UNPACK_ACTION(map_val);
    }

    return 0;
}

#undef KB_SEQ_PACK
#undef KB_SEQ_UNPACK_ID
#undef KB_SEQ_UNPACK_ACTION

int keybind_eval(struct keybind_state *st, char c)
{
    const struct keybind_map *kbm = &keybind_map;
    uint8_t key = c;

    if (st->_seq0) {
        // had previosly first key of a sequence

        if (!btable_get(kbm->seq1_tbl, key)) {
            // was not a complete sequence - "flush" both chars
            st->_seq0 = 0;
            return K_ACTION_UNCACHE;
        }

        uint8_t action = kb_seq_get_action(kbm, st->_seq0, key);
        LOG_DBG("key:0x%02X,0x%02X --> action:%d", st->_seq0, key, action);

        return action;
    }

    if (btable_get(kbm->ckey_tbl, key)) {
        // have mapped key
        if (btable_get(kbm->seq0_tbl, key)) {
            // have first key of sequence mapping
            st->_seq0 = key;
            // up to caller to cache this char
            return K_ACTION_CACHE;
        }

        // have single key mapping
        uint8_t action = kb_get_action(kbm, key);
        if (!action) {
            // should not occur!?
            return K_ACTION_PUTC;
        }

        LOG_DBG("key:0x%02X --> action:%d", key, action);
        return action;
    }

    return K_ACTION_PUTC;
}

/**
 * parse string in format like "C-c" (Control + C) to VT100 compatible value.
 * also accept "ESC"
 */
static int _parse_vtkey(const char *s, uint8_t *vtkey)
{
    uint8_t vtkey_tmp = 0;
    switch (*s) {
        case 'C':
            s++;
            if (*s != '-')
                return -EINVAL;

            s++;
            if (!islower(*s))
                return -EINVAL;

            vtkey_tmp = VT_C_TO_CTRL_KEY(*s);

            s++;
            if (*s != '\0')
                return -EINVAL;
            break;
#if 0
        case 'e':
        case 'E':
            if (strcasecmp("esc", s)) {
                return STR_EINVAL;
            }
            vtkey_tmp = VT_ESC_KEY;
        break;
#endif
        default:
            return -EINVAL;
            break;
    }

    *vtkey = vtkey_tmp;
    return 0;
}

static int parse_bind(const struct opt_conf *conf, char *s)
{
    return -1; // TODO
}

static int keybind_post_parse(const struct opt_section_entry *entry)
{
    int err;
    // note: do not use LOG here. not initialized yet
    //
    // TODO default from

    struct keybind_map *kbm = &keybind_map;
    err = kb_add_action(kbm, VT_C_TO_CTRL_KEY('C'), K_ACTION_EXIT);
    assert(!err);

    err = kb_add_action(kbm, VT_C_TO_CTRL_KEY('D'), K_ACTION_EXIT);
    assert(!err);

    err = kb_add_action(kbm, VT_C_TO_CTRL_KEY('B'), K_ACTION_TOG_CMD_MODE);
    assert(!err);

    err = kb_seq_add_action(kbm, VT_C_TO_CTRL_KEY('A'), 'c',
                            K_ACTION_TOG_CMD_MODE);
    assert(!err);

    return 0;
}

static const struct opt_conf keybind_opts_conf[] = {
    {
        .name = "bind",
        .parse = parse_bind,
        .descr = "bind key to action"
    }
};

OPT_SECTION_ADD(keybind, keybind_opts_conf, ARRAY_LEN(keybind_opts_conf),
                keybind_post_parse);
