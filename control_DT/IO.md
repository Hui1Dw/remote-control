# 当前代码 IO 口清单

> **适用范围：** STM32G431CBTx（LQFP48）遥控器发端固件。本文以 `Core/Src/gpio.c`、
> `Core/Src/adc.c`、`Core/Src/joystick.c`、`Core/Src/usart.c`、`Core/Src/main.c` 和
> `Core/Inc/main.h` 的**当前实际代码**为准。

## 使用概览

| 类别 | 数量 | 引脚 |
|---|---:|---|
| 摇杆模拟输入 | 4 | PA0、PA1、PA2、PA4 |
| 普通输入 | 15 | PA3、PA5、PA6、PA7、PA8、PB0、PB5、PA11、PA12、PA15、PB9、PB12、PB13、PC13、PC14 |
| 普通推挽输出 | 6 | PB1、PB2、PB6、PB7、PB14、PB15 |
| UART 复用功能 | 6 | PA9、PA10、PB3、PB4、PB10、PB11 |
| 时钟/调试保留 | 4 | PF0、PF1、PA13、PA14 |

已配置的应用 IO 之间没有复用冲突。

## 双摇杆 ADC 输入

四路摇杆输入均为 **0～3.3 V 单端模拟量**，使用 12 位 ADC，原始数值范围为 0～4095。
启动时执行 ADC 硬件校准，并在前约 320 ms 自动记录四轴中位值；主循环以 100 Hz 采集原始值、一阶低通滤波，并按记录的中位值映射为 `-100～+100`。其无线组包格式见 [`无线控制帧协议.md`](无线控制帧协议.md)；当前代码会将稳定值经 E22-1 / USART1 发送。

| 信号/宏 | 引脚 | ADC 通道 | 当前采样顺序 | 软件字段 |
|---|---|---|---:|---|
| 左摇杆 X：`JOY_LEFT_X` | **PA0** | ADC1_IN1 | ADC1 第 1 路 | `debug.joystick.raw.left_x` |
| 左摇杆 Y：`JOY_LEFT_Y` | **PA1** | ADC1_IN2 | ADC1 第 2 路 | `debug.joystick.raw.left_y` |
| 右摇杆 X：`JOY_RIGHT_X` | **PA2** | ADC1_IN3 | ADC1 第 3 路 | `debug.joystick.raw.right_x` |
| 右摇杆 Y：`JOY_RIGHT_Y` | **PA4** | ADC2_IN17 | ADC2 第 1 路 | `debug.joystick.raw.right_y` |

`debug.joystick.filtered` 保存一阶低通滤波后的 ADC 值（约 50% 上次值、50% 新样本）。`debug.joystick.value` 保存对应的中心校准映射结果（`int16_t`）：ADC `0` → `-100`、自动记录的各轴中位值 → `0`、ADC `4095` → `+100`。上电后约 320 ms 内请保持摇杆居中；期间输出保持为 `0`。当前不包含死区或端点行程校准。

详细的遥控器输入约束和软件行为见 [`Core/Src/controlIO.md`](Core/Src/controlIO.md)。

## 四路两档拨杆输入

八个拨杆触点均为 `GPIO_MODE_INPUT` + `GPIO_PULLUP`，通过开关接地后读取低电平。`Core/Src/toggle_switch.c` 每 10 ms 解码一次，结果保存在 `debug.toggle_switch.state`；组包时使用该稳定值，并经 E34-1 / USART1 发送。

| 拨杆 | A 触点 / 引脚 | B 触点 / 引脚 | A=低、B=高 | A=高、B=高 | A=高、B=低 | A=低、B=低 |
|---|---|---|---|---|---|---|
| 1 | `A_1` / **PB12** | `B_1` / **PB13** | A 档（`-1`） | 无效（`2`） | B 档（`1`） | 无效（`2`） |
| 2 | `A_2` / **PA15** | `B_2` / **PB9** | A 档（`-1`） | 无效（`2`） | B 档（`1`） | 无效（`2`） |
| 3 | `A_3` / **PC14** | `B_3` / **PC13** | A 档（`-1`） | 无效（`2`） | B 档（`1`） | 无效（`2`） |
| 4 | `A_4` / **PA12** | `B_4` / **PA11** | A 档（`-1`） | 无效（`2`） | B 档（`1`） | 无效（`2`） |

`A=高、B=高` 与 `A=低、B=低` 都不是两档拨杆的有效稳定状态，代码将两者标记为 `TOGGLE_SWITCH_POSITION_INVALID`。拨杆状态已进行软件消抖：新的解码状态连续 3 次 100 Hz 扫描一致后才更新稳定值 `debug.toggle_switch.state`，典型延迟约 20～30 ms；直接解码值可通过 `debug.toggle_switch.raw` 调试观察。PC13/PC14 已占作 GPIO，不能同时启用 LSE 外部低速晶振。

## 四个按键输入

四个按键均使用 `GPIO_MODE_INPUT + GPIO_PULLUP`。默认释放时输入为高电平；按下时触点接地，输入为低电平。`Core/Src/button.c` 在每次 100 Hz 输入扫描中读取并消抖；每次有效按下会翻转对应的锁存状态，组包时使用 `debug.button.toggle` 并经 E34-1 / USART1 持续发送。

| 按键 | MCU 引脚 | 位置/用途 | 释放状态 | 按下状态 | 物理消抖字段 | 锁存发送字段 |
|---|---|---|---|---|---|
| `SW1` | **PA3** | 右摇杆下方按键 | `BUTTON_RELEASED` (`0`) | `BUTTON_PRESSED` (`1`) | `debug.button.physical.sw1` | `debug.button.toggle.sw1` |
| `SW2` | **PA5** | 左摇杆下方按键 | `BUTTON_RELEASED` (`0`) | `BUTTON_PRESSED` (`1`) | `debug.button.physical.sw2` | `debug.button.toggle.sw2` |
| `SW3` | **PA6** | 其他按键 | `BUTTON_RELEASED` (`0`) | `BUTTON_PRESSED` (`1`) | `debug.button.physical.sw3` | `debug.button.toggle.sw3` |
| `SW4` | **PA7** | 其他按键 | `BUTTON_RELEASED` (`0`) | `BUTTON_PRESSED` (`1`) | `debug.button.physical.sw4` | `debug.button.toggle.sw4` |

新的按下或释放状态必须连续 **3 次** 100 Hz 扫描一致，才会写入 `debug.button.physical`，典型消抖延迟约 **20～30 ms**。每次 `debug.button.physical` 从释放变为按下时，对应 `debug.button.toggle` 在 `BUTTON_TOGGLE_OFF`（`0`）和 `BUTTON_TOGGLE_ON`（`1`）之间翻转；上电默认全关，松手后锁存状态保持。未消抖的直接读取结果可通过 `debug.button.raw.sw1` 至 `sw4` 观察。

## E22 无线模块接口

板上焊的是三路不同频段 E22 LoRa（半双工）：USART1=`E22-400`，USART2=`E22-900`，USART3=`E22-230`。CubeMX 初始串口仍是 **9600 8N1**，因为 E22 配置模式的 UART **固定 9600**。`App_Init` 里 `E22Provisioning_Run()` 会：

1. `M0=0`、`M1=1` 进配置模式（不要用 `M0=M1=1`，那是 E34）；
2. 9600 写 `C0 00 09 ...`，目标 `REG0=0xE6`（工作串口 115200 8N1 + 空中 38.4 kbps）、`REG3=0x00` 透传；
3. 回到透传 `M0=M1=0` 后，仅对 C1 成功的通道把 MCU `set_baud(115200)`。

控制帧仍只走 **USART1**。USART1 配置成功后发送周期 10 ms，失败则保持 9600 / 20 ms。USART2、USART3 参与上电配置，暂不发送控制帧，也未启动接收。

### 模块 1：USART1 / E22-400

| E22 信号 | MCU 引脚 | 配置 | 当前用途 |
|---|---|---|---|
| MCU TX / 模块 RXD | **PA9** | USART1_TX，AF7、无上下拉、低速 | 控制帧主发送链路。|
| MCU RX / 模块 TXD | **PA10** | USART1_RX，AF7、无上下拉、低速 | 上电配置时收 C1；运行时不接收控制帧。|
| `M0` | **PB15** | 推挽输出、无上下拉、低速 | 上电先拉低；配置时保持 0。|
| `M1` | **PB14** | 推挽输出、无上下拉、低速 | 上电先拉低；配置时拉高。|
| `AUX` | **PA8** | 普通输入、无上下拉 | 配置和发送前都必须为高。|

### 模块 2：USART2 / E22-900

| E22 信号 | MCU 引脚 | 配置 | 当前用途 |
|---|---|---|---|
| MCU TX / 模块 RXD | **PB4** | USART2_TX，AF7、无上下拉、低速 | 仅上电写寄存器。|
| MCU RX / 模块 TXD | **PB3** | USART2_RX，AF7、无上下拉、低速 | 仅上电收 C1。|
| `M0_2` | **PB7** | 推挽输出、无上下拉、低速 | 上电先拉低；配置时保持 0。|
| `M1_2` | **PB6** | 推挽输出、无上下拉、低速 | 上电先拉低；配置时拉高。|
| `AUX_2` | **PB5** | 普通输入、无上下拉 | 上电配置时等待空闲。|

### 模块 3：USART3 / E22-230

| E22 信号 | MCU 引脚 | 配置 | 当前用途 |
|---|---|---|---|
| MCU TX / 模块 RXD | **PB10** | USART3_TX，AF7、无上下拉、低速 | 仅上电写寄存器。|
| MCU RX / 模块 TXD | **PB11** | USART3_RX，AF7、无上下拉、低速 | 仅上电收 C1。|
| `M0_3` | **PB1** | 推挽输出、无上下拉、低速 | 上电先拉低；配置时保持 0。|
| `M1_3` | **PB2** | 推挽输出、无上下拉、低速 | 上电先拉低；配置时拉高。|
| `AUX_3` | **PB0** | 普通输入、无上下拉 | 上电配置时等待空闲。|

`M0/M1` 的六个控制脚会在 GPIO 模式配置前先写为低电平，因此三个 E22 模块上电默认透传；随后由 `E22Provisioning_Run()` 临时进配置模式。

## 按端口查看

| 端口 | 引脚 | 功能 |
|---|---|---|
| GPIOA | PA0 / PA1 / PA2 / PA4 | 左摇杆 X / 左摇杆 Y / 右摇杆 X / 右摇杆 Y（ADC） |
| GPIOA | PA3 / PA5 / PA6 / PA7 | `SW1` / `SW2` / `SW3` / `SW4` 按键（内部上拉输入、低有效） |
| GPIOA | PA8 | E22-1 `AUX` 输入 |
| GPIOA | PA11 / PA12 / PA15 | 拨杆 4：`B_4` / `A_4`；拨杆 2：`A_2`（内部上拉输入） |
| GPIOA | PA9 / PA10 | USART1 TX / RX |
| GPIOA | PA13 / PA14 | SWDIO / SWCLK（下载调试） |
| GPIOB | PB0 / PB1 / PB2 | E22-3 `AUX` / `M0` / `M1` |
| GPIOB | PB3 / PB4 | USART2 RX / TX |
| GPIOB | PB5 / PB6 / PB7 | E22-2 `AUX` / `M1` / `M0` |
| GPIOB | PB9 / PB12 / PB13 | 拨杆 2：`B_2`；拨杆 1：`A_1` / `B_1`（内部上拉输入） |
| GPIOB | PB10 / PB11 | USART3 TX / RX |
| GPIOB | PB14 / PB15 | E22-1 `M1` / `M0` |
| GPIOC | PC13 / PC14 | 拨杆 3：`B_3` / `A_3`（内部上拉输入；不可同时用作 LSE） |
| GPIOF | PF0 / PF1 | HSE 外部高速晶振 OSC_IN / OSC_OUT |

## CubeMX 配置注意事项

`WireSwitch.ioc` 已记录双摇杆 ADC、四路两档拨杆及四个按键的 GPIO 输入。ADC HAL 驱动、摇杆采样、拨杆解码和按键消抖代码也已加入工程。

USART2、USART3 以及模块 2/3 的 GPIO 仍是历史手动扩展配置：

- `Core/Src/usart.c`：USART2（PB4/PB3）和 USART3（PB10/PB11）；
- `Core/Src/gpio.c`：PB0、PB1、PB2、PB5、PB6、PB7。

若在 CubeMX 中重新生成代码，必须先将这些手动扩展配置补入 `.ioc`，或在重新生成后核对并恢复上述代码；否则模块 2/3 的 IO 初始化可能被覆盖或丢失。

## 未列出的引脚

本文未列出的 MCU 引脚没有被当前应用代码配置或使用。新增功能前应同时检查：复用功能、启动/调试保留脚、电气连接，以及与本表的冲突。
