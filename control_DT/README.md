# WirelessSwitch 无线遥控器发端固件

本工程运行在 STM32G431，正在由原急停发端改造成双摇杆无线遥控器。三路不同频段 E22 LoRa 分别挂在 **USART1、USART2、USART3**；控制帧协议见 [`无线控制帧协议.md`](无线控制帧协议.md)，当前只经 **E22-1 / USART1** 发送。上电会先配置三路 E22（工作串口目标 115200、空中 38.4 kbps），USART1 成功后按 10 ms 组包。

## 当前运行路径

- 活跃入口：`Core/Src/main.c`
- 摇杆 ADC 初始化：`Core/Src/adc.c`
- 摇杆采样接口：`Core/Src/joystick.c`
- 两档拨杆解码：`Core/Src/toggle_switch.c`
- GPIO 初始化：`Core/Src/gpio.c`
- UART 初始化：`Core/Src/usart.c`
- 遥控器输入定义：[`Core/Src/controlIO.md`](Core/Src/controlIO.md)
- 完整引脚清单：[`IO.md`](IO.md)
- 调试变量与验收步骤：[`调试文档.md`](调试文档.md)；调试器 Watch `debug` 即可展开全部公开状态
- CubeMX 工程：`WireSwitch.ioc`
- 构建：`cmake --preset Debug && cmake --build --preset Debug`
- 固件：`build/Debug/WireSwitch.elf`（唯一官方 ELF，不要再用 Makefile / EIDE 另编一份）

启动后，程序会校准 ADC，并以 **100 Hz** 采集两只摇杆的四个原始模拟量：PA0、PA1、PA2、PA4。上电后约 **320 ms** 会自动采集各轴中位值，因此这段时间应保持摇杆居中。原始结果保存在 `debug.joystick.raw`，低通滤波结果保存在 `debug.joystick.filtered`（两者范围均为 0～4095）；按各轴记录中位值映射的结果保存在 `debug.joystick.value`（范围 -100～+100，定义均见 `Core/Inc/joystick.h`）。

四路两档拨杆已配置为内部上拉输入，并以 100 Hz 解码为 A 档（`-1`）或 B 档（`1`）；两个触点同为高或同为低均为无效值（`2`），且已进行软件消抖。四个瞬时按键 SW1～SW4 也已配置为内部上拉、低有效输入并完成软件消抖；每次有效按下都会使对应 `debug.button.toggle` 在关（`0`）/开（`1`）之间翻转，松手后保持，上电默认全关。输入组包、CRC、E22 发送约束和接收端校验要求已定义在 [`无线控制帧协议.md`](无线控制帧协议.md)。当前代码会将稳定输入组装后经 E22-1 / USART1 中断发送。

三个 E22 的 `M0/M1` 上电先拉低（透传），随后由 `E22Provisioning_Run()` 临时进配置模式写寄存器。原 PA6 急停按键、外部中断及 `ABCD` / `1234` 测试报文已移除；PA6 现用作 SW3 按键输入。

> `WireSwitch.ioc` 已记录 ADC1/ADC2 与四路摇杆引脚；USART2、USART3 及模块 2/3 GPIO 仍有历史手动扩展配置。重新生成 CubeMX 代码前请阅读 [`IO.md`](IO.md)。

## 调试步骤

1. 确认两个摇杆使用 3.3 V 供电，并与 MCU 共地。
2. 上电后先保持摇杆居中约 320 ms，确认 `debug.joystick.calibration.valid` 变为 `1`；随后在调试器中观察 `debug.joystick.raw`、`debug.joystick.filtered` 和 `debug.joystick.value`。中位的 `debug.joystick.value` 应接近 0，满行程应接近 -100 和 +100。
3. 在调试器中观察 `debug.toggle_switch.state.switch_1` 至 `switch_4`：A 档为 `-1`，B 档为 `1`；两个触点同时为高或同时为低均会显示无效值 `2`。
4. 观察 `debug.button.physical.sw1` 至 `sw4`：释放为 `0`，按下为 `1`；再观察 `debug.button.toggle.sw1` 至 `sw4`，每次有效按下应在 `0` / `1` 间翻转并在松手后保持。物理状态更新经过约 20～30 ms 消抖。
5. 先看 `debug.e22.ok[0]` 与 `debug.e22.mcu_baud[0]`；USART1 成功后 `tx_period_ms` 应为 10，`tx_success_count` 约 100 Hz。再按 [`无线控制帧协议.md`](无线控制帧协议.md) 核对 9 字节控制帧、序号低 4 位、CRC-8 和接收端失联保护。

## 相关文档

- [`Core/Src/controlIO.md`](Core/Src/controlIO.md)：已确认的摇杆 IO、电气约束和采样行为。
- [`IO.md`](IO.md)：当前代码实际使用的所有 IO 与 CubeMX 注意事项。
- [`无线控制帧协议.md`](无线控制帧协议.md)：输入稳定值的组包格式、CRC、E22/UART 发送约束及联调验收。
- `../PowerManage/docs/PowerManage控制逻辑说明.md`：接收、失联和电源状态机。
- `../wireless_change_channel/`：E34 地址/信道配置专用工程，不是本工程的常规运行路径。
