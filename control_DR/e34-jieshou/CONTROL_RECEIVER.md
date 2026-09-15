# 控制帧接收机实现说明

本工程保持为**接收机**：USART1、USART2、USART3 仍分别从三个 E34 无线模块接收数据，UART 参数和引脚初始化未更改。原有 `ABCD` / `1234` 急停字符串状态机已经移除。
UART DMA 的首个空闲接收事件也会正常提交解析，不会再丢弃发送端的第一帧。

## 实现位置

- `Core/Src/control_protocol.c`：14 字节控制帧的流式组帧、固定字段检查、CRC-16/MODBUS、字段合法性检查。
- `Core/Src/receiver_control.c`：三路独立协议解析、冗余链路选择、失效保护和控制输出。
- `Core/Src/main.c`：保留原 DMA/UART 接收路径；每个收到的字节进入 `ReceiverControl_PushByte()`。

## 帧验证

每一路 UART 都有独立解析器，支持连续数据、半帧和噪声中的 `A5 5A` 重同步。仅在满足以下条件后保存控制帧：

1. 固定帧头 `A5 5A`；
2. 版本、类型、载荷长度依次为 `01`、`01`、`06`；
3. 对偏移 `2..11` 的 10 个字节计算 CRC-16/MODBUS（初值 `0xFFFF`、多项式 `0xA001`）；
4. CRC 与低字节在前的 `CRC_L CRC_H` 一致。

此外，接收机将摇杆范围限制为 `-100..100`，四个拨杆均限制为 `01`（A）或 `10`（B），并要求 `BTN.bit4=1`、`BTN.bit5..7=0`。

## 冗余与安全策略

沿用原工程的链路优先级：**USART2 > USART1 > USART3**。选择在 300 ms 内收到完整 CRC 正确帧的最高优先级链路。

- 上电或所有链路失联：安全状态；
- 当前选中链路发生 CRC 错误：立即安全状态，直到该链路再次给出有效帧；
- 当前选中链路的拨杆为 `00/11`、校准标志无效、保留按键位非零，或摇杆超范围：安全状态；
- 选中链路故障时不会自动采用低优先级链路，避免故障期间控制源悄然切换；链路超时后，系统才按优先级重新选择在线链路。

安全状态的输出为：四个摇杆置零、`SW=0`、`BTN=0`，并将 `receiver_control_safe` 置 1。

## 控制输出接入

当前硬件输出映射未在协议中提供，因此解码后的状态保存在以下可在调试器中观察的变量中：

- `receiver_control_output.left_y / left_x / right_y / right_x`
- `receiver_control_output.switches`
- `receiver_control_output.buttons`
- `receiver_control_output.sequence`
- `receiver_control_safe`
- `receiver_control_active_source`

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

以下数组下标 `0/1/2` 分别对应 USART1/USART2/USART3。只有帧头、版本、类型、长度和 CRC 均正确后才会更新：

```c
receiver_control_last_frame[0].sequence
receiver_control_last_frame[0].left_y
receiver_control_last_frame[0].left_x
receiver_control_last_frame[0].right_y
receiver_control_last_frame[0].right_x
receiver_control_last_frame[0].switches
receiver_control_last_frame[0].buttons
```

- `receiver_control_last_frame_valid[x] == 1`：该路至少收到了一个 CRC 正确的完整帧；
- `receiver_control_last_frame_safe[x] == 1`：该帧的摇杆范围、4 个拨杆编码和 `BTN.bit4` 也均通过安全检查；
- `receiver_control_output`：当前被优先级选中的、允许实际控制的最终输出；若安全状态触发，它会被清零。

为了在 Watch 中免除位运算，还提供了逐个解码的数组：

```c
receiver_control_switch_state[0][0]  /* USART1 的拨杆1 */
receiver_control_switch_state[0][1]  /* USART1 的拨杆2 */
receiver_control_switch_state[0][2]  /* USART1 的拨杆3 */
receiver_control_switch_state[0][3]  /* USART1 的拨杆4 */

receiver_control_button_state[0][0]  /* USART1 的按键1 */
receiver_control_button_state[0][1]  /* USART1 的按键2 */
receiver_control_button_state[0][2]  /* USART1 的按键3 */
receiver_control_button_state[0][3]  /* USART1 的按键4 */
receiver_control_calibration_valid[0]
```

拨杆值：`1=A`、`2=B`、`0/3=保留或异常`；按键值：`0=关`、`1=开`。将第一维改为 `[1]` 或 `[2]` 可查看 USART2 或 USART3。
- `receiver_control_safe_reason`：安全状态原因：`1=无在线有效源/超时`，`2=选中链路出现 CRC 错`，`3=选中帧的拨杆、校准位或摇杆范围无效`；
- `receiver_control_selected_source`：按优先级选出的候选串口，即使因安全检查拒绝输出时也保留其编号，便于定位。

## 当前 UART 硬件引脚

- USART1：`PA9=TX`、`PA10=RX`，不使用 USART SWAP；
- USART2：`PB3=TX`、`PB4=RX`；
- USART3：`PB10=TX`、`PB11=RX`。

`PA9` 与旧工程的 I2C2 SCL 复用冲突，因此本固件不初始化 I2C2，避免其重新占用 PA9。原先读取 `PB3` 的 AUX 调试输入也已禁用，因为 `PB3` 现在为 USART2_TX；如需监测该无线模块的 AUX，请提供新的 AUX 实际连接引脚。

## UART 接收诊断变量

以下数组的下标 `0/1/2` 分别对应 USART1/USART2/USART3，且不依赖控制帧 CRC 是否正确：

- `uart_rx_dma_event_count[]`：UART IDLE/DMA 接收事件次数；
- `uart_rx_last_dma_size[]`：最近一次事件对应的 DMA 字节数；
- `uart_rx_dma_byte_count[]`：已提交给协议层的原始字节累计数；
- `uart_rx_empty_event_count[]`：DMA 刚启动时的零字节 IDLE 事件次数；
- `uart_rx_dma_live_byte_count[]`：DMA 当前缓冲中已搬运、但尚未等到 IDLE 提交的字节数；
- `uart_rx_dma_ndtr[]`：DMA 当前剩余传输计数，正常刚启动时为 256；
- `uart_rx_dma_start_ok_count[]` / `uart_rx_dma_start_error_count[]`：Receive-to-IDLE DMA 启动成功/失败次数；
- `uart_rx_last_dma_start_status[]`：最近一次 DMA 启动的 HAL 返回值，`0` 为 `HAL_OK`；
- `uart_rx_error_callback_count[]` / `uart_rx_last_error_code[]`：HAL 接收错误回调次数及最近错误码。错误后固件会自动重启该路 DMA 接收。

若 `uart_rx_dma_event_count[x]` 保持为零、但 `uart_rx_dma_live_byte_count[x]` 增加，发送端正在连续发送而没有形成 IDLE 间隔；此时 DMA 已经收到了数据。两者都为零时，数据没有到达 MCU 的对应 RX 引脚，或 DMA 接收未成功启动。若 DMA 字节计数增加但 `receiver_control_valid_frame_count[x]` 不增加，则串口有数据但协议字段或 CRC 不正确。

### UART 硬件接收诊断

以下数组下标 `0/1/2` 分别代表 USART1、USART2、USART3：

- `uart_rx_irq_count[]`：UART 外设中断次数；持续为 0 表示 MCU 的该 RX 引脚没有触发 UART 中断；
- `uart_rx_dma_event_count[]`：HAL Receive-to-IDLE DMA 的有效接收事件次数；
- `uart_rx_last_dma_size[]`：最近一次实际收到的字节数；控制帧通常为 14；
- `uart_rx_dma_byte_count[]`：通过 DMA 交给上层的累计字节数；
- `uart_rx_error_irq_count[]`：奇偶、帧、噪声或溢出错误中断次数；
- `uart_rx_last_isr[]`：最近一次 UART ISR 寄存器快照，供进一步硬件排查。

`uart_rx_irq_count[0]` 在上电时偶发地为 1，可能只是 USART1 初始化前已经置位的 IDLE/错误状态，而不是收到一个字节；必须以 `uart_rx_dma_event_count[]`、`uart_rx_dma_byte_count[]` 或 `uart_rx_dma_live_byte_count[]` 判断是否真的收到数据。固件会在每次重启 DMA 前清除这类陈旧标志，不再将其解释为 255/256 字节数据。
