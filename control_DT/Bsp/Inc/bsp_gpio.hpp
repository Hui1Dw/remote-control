#ifndef BSP_GPIO_HPP
#define BSP_GPIO_HPP

extern "C" {
#include "main.h"
}

#include <cstdint>

namespace BSP_n
{

enum class GPIOPull_t : uint8_t
{
    NONE,
    UP,
    DOWN,
};

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

    /** 临时重配为输入，用于无仪器线路探测。 */
    void configure_input(GPIOPull_t pull);

    /** 恢复为推挽复用功能。 */
    void configure_alternate(uint32_t alternate);

private:
    GPIO_TypeDef *port_;
    uint16_t pin_;
};

} /* namespace BSP_n */

#endif /* BSP_GPIO_HPP */
