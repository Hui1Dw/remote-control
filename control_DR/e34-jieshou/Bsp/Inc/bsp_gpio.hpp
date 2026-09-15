#ifndef BSP_GPIO_HPP
#define BSP_GPIO_HPP

extern "C" {
#include "main.h"
}

#include <cstdint>

namespace BSP_n
{

/**
 * @brief 单个 GPIO 引脚包装。
 *
 * 只保存端口和引脚。构造时不改 CubeMX 已完成的模式配置。
 */
class GPIO_c
{
public:
    GPIO_c(GPIO_TypeDef *port, uint16_t pin);

    bool is_set() const;
    bool is_reset() const;
    void set();
    void reset();
    void write(bool high);

private:
    GPIO_TypeDef *port_;
    uint16_t pin_;
};

} /* namespace BSP_n */

#endif /* BSP_GPIO_HPP */
