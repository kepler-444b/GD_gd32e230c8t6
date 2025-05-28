#if 0
#ifndef _CONFIG_H_
#define _CONFIG_H_

typedef struct __attribute__((aligned(4)))
{
    uint8_t head;
    uint8_t func[4];
    uint8_t group[4];
    uint8_t area[4];
    uint8_t perm[4];
    uint8_t scene_group[4];

    uint8_t func_5;
    uint8_t group_5;
    uint8_t area_5;
    uint8_t perm_5;
    uint8_t scene_group_5;

    uint8_t func_6;
    uint8_t gruop_6;
    uint8_t area_6;
    uint8_t perm_6;
    uint8_t scene_group_6;

    uint8_t crc;
    uint8_t reco_h;
    uint8_t reco_l;
    uint8_t channel;
    uint8_t cate;
} dev_config;

extern dev_config my_dev_config;

#endif
#endif