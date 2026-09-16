#ifndef BSP_H
#define BSP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* C 包装：现有 .c 文件通过这里取时间，不直接调用 HAL_GetTick。 */

uint32_t Bsp_NowMs(void);
void Bsp_DelayMs(uint32_t ms);

uint8_t Bsp_UsbWrite(const uint8_t *data, uint16_t len);
uint16_t Bsp_UsbRead(uint8_t *data, uint16_t max_len);
uint8_t Bsp_UsbConfigured(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_H */
