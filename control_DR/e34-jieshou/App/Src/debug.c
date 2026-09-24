#include "debug.h"

/* 接收端公开调试状态。调试器 Watch `debug` 即可展开全部字段。 */
volatile Debug_t debug = {
    .safe = 1U,
    .selected_source = RECEIVER_CONTROL_SOURCE_NONE,
    .active_source = RECEIVER_CONTROL_SOURCE_NONE,
    .safe_reason = RECEIVER_CONTROL_SAFE_REASON_NO_ONLINE_SOURCE,
};
