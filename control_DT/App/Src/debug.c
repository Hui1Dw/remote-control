#include "debug.h"

/* 发端公开调试状态。调试器 Watch `debug` 即可展开全部字段。 */
volatile Debug_t debug = {
    .toggle_switch = {
        .raw = {
            TOGGLE_SWITCH_POSITION_INVALID,
            TOGGLE_SWITCH_POSITION_INVALID,
            TOGGLE_SWITCH_POSITION_INVALID,
            TOGGLE_SWITCH_POSITION_INVALID},
        .state = {
            TOGGLE_SWITCH_POSITION_INVALID,
            TOGGLE_SWITCH_POSITION_INVALID,
            TOGGLE_SWITCH_POSITION_INVALID,
            TOGGLE_SWITCH_POSITION_INVALID}}};
