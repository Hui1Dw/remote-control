# 控制帧接收机实现说明

本工程保持为**接收机**：USART1、USART2、USART3 分别从三路 E22 接收同一 9 字节控制帧。CubeMX UART 仍从 9600 启动；`App_Init` 先配置 E22 再启动 DMA。原有 `ABCD` / `1234` 急停字符串状态机已经移除。
UART DMA 的首个空闲接收事件也会正常提交解析，不会再丢弃发送端的第一帧。

## 实现位置

- `App/Src/app.c`：`App_Init` 调用 `E22Provisioning_Run()`，成功后再 `Board_UartStartRx()`。
- `App/Src/e22_provisioning.c`：上电一次性写 E22 寄存器；只走 Board，不碰 HAL 句柄。
- `Board/Src/board.cpp`：`HAL_UARTEx_RxEventCallback` 经 `USART_c` 直接 `ReceiverControl_PushBuffer()`。
- `App/Src/control_protocol.c`：9 字节控制帧的流式组帧、版本半字节检查、CRC-8/MAXIM、字段合法性检查。
- `App/Src/receiver_control.c`：三路独立协议解析、冗余链路选择、失效保护和控制输出。
- `Core/Src/main.c`：USER CODE 只调用 `Board_Init` / `App_Init` / `App_Loop`。

## 帧验证

每一路 UART 都有独立解析器，支持连续数据、半帧和噪声中的 `A5` 重同步。仅在满足以下条件后保存控制帧：

1. 帧头 `A5`；
2. 下一字节高 4 位为版本 `2`，低 4 位为序号 `0～15`。版本不对则从当前字节重新搜帧头，不记格式错误，也不进入失联安全态；
3. 收满 9 字节后，对偏移 `1..7` 计算 CRC-8/MAXIM（初值 `0x00`、反射多项式 `0x8C`）；
4. CRC 与偏移 8 的单字节一致。

此外，接收机将摇杆范围限制为 `-100..100`，四个拨杆均限制为 `01`（A）或 `10`（B），并要求 `BTN.bit4=1`、`BTN.bit5..7=0`。

## 冗余与安全策略

沿用原工程的链路优先级：**USART2 > USART1 > USART3**。选择在 300 ms 内收到完整 CRC 正确帧的最高优先级链路。

- 上电或所有链路失联：安全状态；
- 当前选中链路发生 CRC 错误：立即安全状态，直到该链路再次给出有效帧；
- 当前选中链路的拨杆为 `00/11`、校准标志无效、保留按键位非零，或摇杆超范围：安全状态；
- 选中链路故障时不会自动采用低优先级链路，避免故障期间控制源悄然切换；链路超时后，系统才按优先级重新选择在线链路。

安全状态的输出为：四个摇杆置零、`SW=0`、`BTN=0`，并将 `debug.safe` 置 1。

## 控制输出接入

当前硬件输出映射未在协议中提供，因此解码后的状态保存在全局 `debug` 中，调试器 Watch 该变量即可：

- `debug.output.left_y / left_x / right_y / right_x`
- `debug.output.switches`
- `debug.output.buttons`
- `debug.output.sequence`
- `debug.safe`
- `debug.active_source`

在其他业务模块中实现同名强符号即可接入实际执行机构：

```c
void ReceiverControl_Apply(const ReceiverControl_Output *output)
{
    /* 将状态量写入电机/继电器/上位机接口；不要把 BTN 当单次事件。 */
}

void ReceiverControl_ApplySafeState(void)
{
    /* 将实际执行机构置为预设安全状态。 */
}
```

默认弱实现为空，避免在未明确实际执行机构接口时误发出旧急停 I2C 命令。`ReceiverControl_Apply()` 只会在摇杆、拨杆、按键状态或选中无线链路改变时调用，因此按键保持为 1 的重复帧不会重复触发动作。


## 已解析控制数据（调试器直接观察）

调试器 Watch `debug`。以下数组下标 `0/1/2` 分别对应 USART1/USART2/USART3。只有帧头、版本半字节和 CRC-8 均正确后才会更新：

```c
debug.link[0].last_frame.sequence
debug.link[0].last_frame.left_y
debug.link[0].last_frame.left_x
debug.link[0].last_frame.right_y
debug.link[0].last_frame.right_x
debug.link[0].last_frame.switches
debug.link[0].last_frame.buttons
```

- `debug.link[x].last_frame_valid == 1`：该路至少收到了一个 CRC 正确的完整帧；
- `debug.link[x].last_frame_safe == 1`：该帧的摇杆范围、4 个拨杆编码和 `BTN.bit4` 也均通过安全检查；
- `debug.output`：当前被优先级选中的、允许实际控制的最终输出；若安全状态触发，它会被清零。

为了在 Watch 中免除位运算，还提供了逐个解码的数组：

```c
debug.link[0].switch_state[0]  /* USART1 的拨杆1 */
debug.link[0].switch_state[1]  /* USART1 的拨杆2 */
debug.link[0].switch_state[2]  /* USART1 的拨杆3 */
debug.link[0].switch_state[3]  /* USART1 的拨杆4 */

debug.link[0].button_state[0]  /* USART1 的按键1 */
debug.link[0].button_state[1]  /* USART1 的按键2 */
debug.link[0].button_state[2]  /* USART1 的按键3 */
debug.link[0].button_state[3]  /* USART1 的按键4 */
debug.link[0].calibration_valid
```

拨杆值：`1=A`、`2=B`、`0/3=保留或异常`；按键值：`0=关`、`1=开`。将第一维改为 `[1]` 或 `[2]` 可查看 USART2 或 USART3。
- `debug.safe_reason`：安全状态原因：`1=无在线有效源/超时`，`2=选中链路出现 CRC 错`，`3=选中帧的拨杆、校准位或摇杆范围无效`；
- `debug.selected_source`：按优先级选出的候选串口，即使因安全检查拒绝输出时也保留其编号，便于定位。

## 当前 UART / E22 硬件引脚

| 串口 | MCU TX | MCU RX | AUX | M1 | M0 |
|---|---|---|---|---|---|
| USART1 E22-400 | PA9 | PA10 | PA8 | PB15 | PB14 |
| USART2 E22-900 | PB3 | PB4 | PB5 | PB6 | PB7 |
| USART3 E22-230 | PB10 | PB11 | PB2 | PB1 | PB0 |

USART1 不使用 SWAP。`PA9` 与旧工程 I2C2 SCL 复用，`PA8` 现为 E22-1 AUX，因此不初始化 I2C2。

Watch `debug.aux_usart1/2/3` 和 `debug.e22`：配置成功后 `mcu_baud[]` 应为 115200，`readback_reg0[]` 约 `0xE7`（230 MHz 可能只保证高 5 位 `0xE0`）。

## UART 接收诊断变量

以下数组的下标 `0/1/2` 分别对应 USART1/USART2/USART3，且不依赖控制帧 CRC 是否正确：

- `debug.uart[x].dma_event_count`：UART IDLE/DMA 接收事件次数；
- `debug.uart[x].last_dma_size`：最近一次事件对应的 DMA 字节数；
- `debug.uart[x].dma_byte_count`：已提交给协议层的原始字节累计数；
- `debug.uart[x].empty_event_count`：DMA 刚启动时的零字节 IDLE 事件次数；
- `debug.uart[x].dma_live_byte_count`：DMA 当前缓冲中已搬运、但尚未等到 IDLE 提交的字节数；
- `debug.uart[x].dma_ndtr`：DMA 当前剩余传输计数，正常刚启动时为 256；
- `debug.uart[x].dma_start_ok_count` / `debug.uart[x].dma_start_error_count`：Receive-to-IDLE DMA 启动成功/失败次数；
- `debug.uart[x].last_dma_start_status`：最近一次 DMA 启动的 HAL 返回值，`0` 为 `HAL_OK`；
- `debug.uart[x].error_callback_count` / `debug.uart[x].last_error_code`：HAL 接收错误回调次数及最近错误码。错误后固件会自动重启该路 DMA 接收。

若 `debug.uart[x].dma_event_count` 保持为零、但 `debug.uart[x].dma_live_byte_count` 增加，发送端正在连续发送而没有形成 IDLE 间隔；此时 DMA 已经收到了数据。两者都为零时，数据没有到达 MCU 的对应 RX 引脚，或 DMA 接收未成功启动。若 DMA 字节计数增加但 `debug.link[x].valid_frame_count` 不增加，则串口有数据但协议字段或 CRC 不正确。

### UART 硬件接收诊断

以下数组下标 `0/1/2` 分别代表 USART1、USART2、USART3：

- `debug.uart[x].irq_count`：UART 外设中断次数；持续为 0 表示 MCU 的该 RX 引脚没有触发 UART 中断；
- `debug.uart[x].dma_event_count`：HAL Receive-to-IDLE DMA 的有效接收事件次数；
- `debug.uart[x].last_dma_size`：最近一次实际收到的字节数；定点模式下模块已剥掉地址头，控制帧通常为 9；
- `debug.uart[x].dma_byte_count`：通过 DMA 交给上层的累计字节数；
- `debug.uart[x].error_irq_count`：奇偶、帧、噪声或溢出错误中断次数；
- `debug.uart[x].last_isr`：最近一次 UART ISR 寄存器快照，供进一步硬件排查。

`debug.uart[0].irq_count` 在上电时偶发地为 1，可能只是 USART1 初始化前已经置位的 IDLE/错误状态，而不是收到一个字节；必须以 `debug.uart[x].dma_event_count`、`debug.uart[x].dma_byte_count` 或 `debug.uart[x].dma_live_byte_count` 判断是否真的收到数据。固件会在每次重启 DMA 前清除这类陈旧标志，不再将其解释为 255/256 字节数据。
