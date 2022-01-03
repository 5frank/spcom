#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include "assert.h"
#include "utils.h"
#include "vt_defs.h"
#include "log.h"
#include "opt.h"
#include "keybind.h"

#define BITS256_WORD_NBITS (sizeof(unsigned int) * 8)
#define BITS256_WORD_OFFSET(INDEX) ((INDEX) / BITS256_WORD_NBITS)
#define BITS256_BITPOS(INDEX) ((INDEX) & (BITS256_WORD_NBITS - 1))
#define BITS256_MASK(INDEX) (1 << BITS256_BITPOS(INDEX))

struct bits256 {
    unsigned int bits[(UINT8_MAX / BITS256_WORD_NBITS) + 1];
};

static inline void bits256_reset(struct bits256 *table, int bitval)
{
    memset(table->bits, bitval ? 0xff : 0, sizeof(table->bits));
}

static inline void bits256_set(struct bits256 *table, uint8_t index)
{
    unsigned int offs = BITS256_WORD_OFFSET(index);
    table->bits[offs] |= BITS256_MASK(index);
}

static inline void bits256_clr(struct bits256 *table, uint8_t index)
{
    unsigned int offs = BITS256_WORD_OFFSET(index);
    table->bits[offs] &= ~(BITS256_MASK(index));
}

static inline int bits256_get(const struct bits256 *table, uint8_t index)
{
    unsigned int offs = BITS256_WORD_OFFSET(index);
    return !!(table->bits[offs] & BITS256_MASK(index));
}

struct keybind_data {
    // have key sequence if both int ckey_tbl and seq_tbl
    struct bits256 ckey_tbl;
    struct bits256 seq_tbl;
    uint32_t seq_map[16];
    uint16_t map[16];
    uint8_t seq0; // leader ctrl key
};


static struct keybind_data keybind_data;
static int kb_error(int err, const char *msg)
{
    fprintf(stderr, msg);
    return err;
}

static inline bool kb_is_mapped(struct keybind_data *kbd, uint8_t key)
{
    return bits256_get(&kbd->ckey_tbl, key);
}

static inline bool kb_is_seq_mapped(struct keybind_data *kbd, uint8_t key)
{
    return bits256_get(&kbd->seq_tbl, key);
}

#define KB_UNPACK_KEY(VAL)    ((VAL) & 0xFF)
#define KB_UNPACK_ACTION(VAL) (((VAL) >> 8) & 0xFF)
#define KB_PACK(KEY, ACTION) (0             \
    | (((uint16_t)(KEY)    & 0xFF))         \
    | (((uint16_t)(ACTION) & 0xFF) <<  8)   \
)

static int kb_add_action(struct keybind_data *kbd, uint8_t key, uint8_t action)
{
    assert(key);
    if (bits256_get(&kbd->seq_tbl, key))
        return kb_error(EEXIST, "conflicting key");

    for (int i = 0; i < ARRAY_LEN(kbd->map); i++) {
        uint16_t map_val = kbd->map[i];
        // empty slot
        if (!map_val) {
            kbd->map[i] = KB_PACK(key, action);
            bits256_set(&kbd->ckey_tbl, key);
            return 0;
        }
    }

    return kb_error(ENOMEM, "max limit key bindings");
}
/// get single key binding action
static uint8_t kb_get_action(struct keybind_data *kbd, uint8_t key)
{
    for (int i = 0; i < ARRAY_LEN(kbd->map); i++) {
        uint16_t map_val = kbd->map[i];
        if (!map_val)
            break;

         if (KB_UNPACK_KEY(map_val) == key)
             return KB_UNPACK_ACTION(map_val);
    }

    return 0;
}


#define KB_SEQ_PACK(CTLK, FOLLOWK, ACTION) (0 \
    | (((uint32_t)(CTLK)    & 0xFF) <<  0)    \
    | (((uint32_t)(FOLLOWK) & 0xFF) <<  8)    \
    | (((uint32_t)(ACTION)  & 0xFF) << 16)    \
)
#define KB_SEQ_UNPACK_ID(VAL) ((VAL) & 0xFFFF)
#define KB_SEQ_UNPACK_ACTION(VAL) (((VAL) >> 16) & 0xFF)

static int kb_seq_add_action(struct keybind_data *kbd,
                             uint8_t ctlkey,
                             uint8_t followk,
                             uint8_t action)
{
    assert(ctlkey);
    assert(followk);

    if (bits256_get(&kbd->ckey_tbl, ctlkey))
        return kb_error(EEXIST, "conflicting ctlkey");

    for (int i = 0; i < ARRAY_LEN(kbd->seq_map); i++) {
        uint32_t map_val = kbd->seq_map[i];
        // empty slot
        if (!map_val) {
            kbd->seq_map[i] = KB_SEQ_PACK(ctlkey, followk, action);
            bits256_set(&kbd->ckey_tbl, ctlkey);
            bits256_set(&kbd->seq_tbl, ctlkey);
            return 0;
        }
    }

    return kb_error(ENOMEM, "max limit key bindings");
}


/** get key sequence action
 * return 0 if not unmapped
 */
static uint8_t kb_seq_get_action(struct keybind_data *kbd,
                                 uint8_t ctlkey,
                                 uint8_t followk)
{
    uint32_t id = KB_SEQ_PACK(ctlkey, followk, 0);

    for (int i = 0; i < ARRAY_LEN(kbd->seq_map); i++) {
        uint32_t map_val = kbd->seq_map[i];
        if (!map_val)
            break;

        if (KB_SEQ_UNPACK_ID(map_val) == id)
            return KB_SEQ_UNPACK_ACTION(map_val);
    }

    return 0;
}

int keybind_eval(char c, char *cfwd)
{
    uint8_t key = c;

    struct keybind_data *kbd = &keybind_data;
    if (kbd->seq0) {
        // forward ckey on double press
        if (key == kbd->seq0) {
            kbd->seq0 = 0;
            cfwd[0] = key;
            return K_ACTION_FWD1;
        }

        uint8_t action = kb_seq_get_action(kbd, kbd->seq0, key);
        if (!action) {
            cfwd[0] = kbd->seq0; // i.e. prev char
            cfwd[1] = key;
            kbd->seq0 = 0;
            return K_ACTION_FWD2;
        }

        LOG_DBG("key:0x%02X,0x%02X --> action:%d", kbd->seq0, key, action);
        return action;
    }

    if (kb_is_mapped(kbd, key)) {

        if (kb_is_seq_mapped(kbd, key)) {
            kbd->seq0 = key;
            return 0; // consumed
        }
        // else single key mapped
        else {

            uint8_t action = kb_get_action(kbd, key);
            if (!action) {
                // should not occur!?
                cfwd[0] = key;
                return K_ACTION_FWD1;
            }
            LOG_DBG("key:0x%02X --> action:%d", key, action);

            return action;
        }
    }

    cfwd[0] = key;
    return K_ACTION_FWD1;
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

            vtkey_tmp = VT_CTRL_KEY(*s);

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

    struct keybind_data *kbd = &keybind_data;
    err = kb_add_action(kbd, VT_CTRL_KEY('C'), K_ACTION_EXIT);
    assert(!err);

    err = kb_add_action(kbd, VT_CTRL_KEY('D'), K_ACTION_EXIT);
    assert(!err);

    err = kb_add_action(kbd, VT_CTRL_KEY('B'), K_ACTION_TOG_CMD_MODE);
    assert(!err);

    err = kb_seq_add_action(kbd, VT_CTRL_KEY('A'), 'c', K_ACTION_TOG_CMD_MODE);
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

OPT_SECTION_ADD(keybind,
                keybind_opts_conf,
                ARRAY_LEN(keybind_opts_conf),
                keybind_post_parse);

