# 遥控器输入 IO 定义

> 本文件记录遥控器新增输入的已确认硬件分配。无线模块的 UART、`M0/M1` 和 `AUX` 配置保持不变，详见根目录 [`IO.md`](../../IO.md)。

## 双摇杆 ADC 输入

两个摇杆均为双轴模拟摇杆，输出电压范围为 **0～3.3 V**，与 MCU 使用同一 3.3 V 参考和地。
ADC 采用 12 位、右对齐单端采样，原始结果范围为 **0～4095**；滤波和中心校准后的摇杆值会参与 [`无线控制帧协议.md`](../../无线控制帧协议.md) 控制帧。

| 摇杆 | 轴 | 信号宏 | MCU 引脚 | ADC 外设/通道 | ADC 序列顺序 |
|---|---|---|---|---|---:|
| 左摇杆 | X | `JOY_LEFT_X` | PA0 | ADC1_IN1 | 1 |
| 左摇杆 | Y | `JOY_LEFT_Y` | PA1 | ADC1_IN2 | 2 |
| 右摇杆 | X | `JOY_RIGHT_X` | PA2 | ADC1_IN3 | 3 |
| 右摇杆 | Y | `JOY_RIGHT_Y` | PA4 | ADC2_IN17 | 1 |

## 当前软件行为

- `MX_ADC1_Init()` 配置 ADC1 扫描 PA0、PA1、PA2；`MX_ADC2_Init()` 配置 ADC2 采样 PA4。
- 启动时先执行 ADC 单端硬件校准，随后自动开始四轴**中心校准**。
- 主循环每 **10 ms（100 Hz）** 调用一次 `Joystick_Update()`。
- 上电后前 **32 个采样周期（约 320 ms）** 会记录四个摇杆的中位值；这段时间请保持两个摇杆处于中位。
  - 校准进行时 `debug.joystick.calibration.in_progress` 为 `1`，`debug.joystick.value` 保持为 `0`；
  - 校准完成且中位值有效时，`debug.joystick.calibration.valid` 为 `1`；若无效，可在摇杆回中后调用 `Joystick_StartCalibration()` 重新校准。
- 最新 ADC 原始值保存在 `debug.joystick.raw`，一阶低通滤波后的值保存在 `debug.joystick.filtered`。
  - 滤波器每次保留约 75% 上次结果、加入约 25% 新样本，用于降低摇杆抖动；
  - 两个对象均含 `left_y`、`left_x`、`right_y`、`right_x` 字段。
- `debug.joystick.value` 保存以各轴记录中心值为零点的线性映射结果：ADC `0` 对应 `-100`，中心值对应 `0`，ADC `4095` 对应 `+100`。
- 原 PA6 急停按键、外部中断和 `ABCD` / `1234` 测试发送逻辑均已删除；PA6 现作为 SW3 按键输入。
- `wireless_control.c` 每 20 ms 从 `debug.joystick.value` 读取稳定控制值，经 E34-1 / USART1 发送；中心校准无效时四轴发送为 0，并清除控制帧校准有效位。

## 四路两档拨杆输入

每个拨杆使用两个独立触点：`A_n` 和 `B_n`。八个 MCU 引脚均配置为普通输入、**内部上拉**，因此没有接通触点时均读取高电平。

| 拨杆 | A 触点 | B 触点 | MCU 引脚 |
|---|---|---|---|
| 1 | `A_1` | `B_1` | PB12 / PB13 |
| 2 | `A_2` | `B_2` | PA15 / PB9 |
| 3 | `A_3` | `B_3` | PC14 / PC13 |
| 4 | `A_4` | `B_4` | PA12 / PA11 |

### 档位判定

| A 触点电平 | B 触点电平 | 解码状态 | 枚举值 |
|---|---|---|---:|
| 低 | 高 | A 档 | `TOGGLE_SWITCH_POSITION_A` (`-1`) |
| 高 | 高 | 无效状态（两触点均悬空） | `TOGGLE_SWITCH_POSITION_INVALID` (`2`) |
| 高 | 低 | B 档 | `TOGGLE_SWITCH_POSITION_B` (`1`) |
| 低 | 低 | 无效状态（两触点同时接地） | `TOGGLE_SWITCH_POSITION_INVALID` (`2`) |

- 主循环以 **100 Hz** 调用 `ToggleSwitch_Update()`；直接解码值位于 `debug.toggle_switch.raw.switch_1` 至 `switch_4`，消抖后的稳定状态位于 `debug.toggle_switch.state.switch_1` 至 `switch_4`。
- 已增加软件消抖：新的 A / B / 无效状态必须连续 **3 次** 100 Hz 扫描一致，才会更新稳定状态，典型延迟约 **20～30 ms**。拨动瞬间的触点抖动不会改变上一稳定档位。
- PC13、PC14 当前按 GPIO 输入使用；不能同时启用 LSE（32.768 kHz 外部低速晶振）功能。

## 四个按键输入

四个按键均配置为普通输入、**内部上拉**。默认释放状态为高电平；按下时按键将对应输入接地，因此读取低电平。

| 按键 | 位置/用途 | MCU 引脚 | 释放状态 | 按下状态 |
|---|---|---|---|---|
| `SW1` | 右摇杆下方按键 | PA3 | 高电平 / `BUTTON_RELEASED` (`0`) | 低电平 / `BUTTON_PRESSED` (`1`) |
| `SW2` | 左摇杆下方按键 | PA5 | 高电平 / `BUTTON_RELEASED` (`0`) | 低电平 / `BUTTON_PRESSED` (`1`) |
| `SW3` | 其他按键 | PA6 | 高电平 / `BUTTON_RELEASED` (`0`) | 低电平 / `BUTTON_PRESSED` (`1`) |
| `SW4` | 其他按键 | PA7 | 高电平 / `BUTTON_RELEASED` (`0`) | 低电平 / `BUTTON_PRESSED` (`1`) |

- 主循环以 **100 Hz** 调用 `Button_Update()`。
- `debug.button.raw.sw1` 至 `sw4` 保存直接读取结果；`debug.button.physical.sw1` 至 `sw4` 保存消抖后的物理按键状态。
- 新的按下或释放状态必须连续 **3 次** 100 Hz 扫描一致才会更新物理状态，典型消抖延迟约 **20～30 ms**。
- 每次 `debug.button.physical` 从释放变为按下时，对应 `debug.button.toggle.sw1` 至 `sw4` 在 `BUTTON_TOGGLE_OFF`（`0`）和 `BUTTON_TOGGLE_ON`（`1`）之间翻转；上电均为关，松手后保持。
- `debug.button.toggle` 会参与 E34-1 控制帧的 bit0～bit3，并持续发送，不能将短暂的物理按下状态直接作为控制帧状态。

## 电气约束

1. 摇杆 `VCC` 必须接 3.3 V，不能向 PA0、PA1、PA2、PA4 输入超过 3.3 V 的电压。
2. 摇杆 GND 与 STM32 GND 必须共地。
3. 四个 ADC 脚均配置为模拟模式、无内部上拉/下拉，不能再复用为普通 GPIO 或其他外设功能。
4. 建议每路模拟输出串联约 1 kΩ，并在 ADC 脚对地预留 10～100 nF 滤波电容；具体取值应结合摇杆电位器阻值和实际响应测试确定。

## 后续待定业务

两档拨杆和按钮的**业务功能**仍需与接收端控制逻辑确认；但其物理引脚、电平逻辑和无线控制帧编码已在本文及 [`无线控制帧协议.md`](../../无线控制帧协议.md) 中确定。
