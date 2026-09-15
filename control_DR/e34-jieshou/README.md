# 三路冗余无线遥控接收机

本工程运行在 **STM32G431CBTx**，角色为遥控器的**接收机**。三个 E34 无线模块分别通过 USART1、USART2、USART3 向 MCU 发送相同的 14 字节控制帧。接收机对每一路独立解析、校验 CRC，并根据链路优先级和安全规则输出最终控制状态。

> 本工程不会把收到的控制帧自动映射到电机、继电器或 I2C 外设。实际执行机构应在 `ReceiverControl_Apply()` 与 `ReceiverControl_ApplySafeState()` 中接入，见下文“接入实际输出”。

---

## 1. UART 与无线模块接线

| 串口 | MCU TX | MCU RX | 模块接线（必须交叉） | 诊断数组下标 |
|---|---:|---:|---|---:|
| USART1 | PA9 | PA10 | 模块 TXD → PA10；模块 RXD ← PA9 | `0` |
| USART2 | PB3 | PB4 | 模块 TXD → PB4；模块 RXD ← PB3 | `1` |
| USART3 | PB10 | PB11 | 模块 TXD → PB11；模块 RXD ← PB10 | `2` |

串口参数固定为：

```text
9600 baud，8 data bits，no parity，1 stop bit（9600 8N1）
```

所有模块必须与 MCU **共地**。模块 `M0/M1` 必须处于正常透明收发模式，不能悬空。无线两端的地址、信道、空速和串口参数必须一致。

> `PA9` 已用作 USART1_TX，因此本工程不会初始化旧工程的 I2C2，避免 I2C2_SCL 与 PA9 冲突。

---

## 2. 接收控制帧协议

帧长固定为 **14 字节**：

```text
A5 5A 01 01 SEQ 06 LY LX RY RX SW BTN CRC_L CRC_H
```

| 偏移 | 字段 | 说明 |
|---:|---|---|
| 0～1 | `A5 5A` | 帧头 |
| 2 | `01` | 协议版本 |
| 3 | `01` | 实时控制帧类型 |
| 4 | `SEQ` | 帧序号 |
| 5 | `06` | 载荷长度 |
| 6 | `LY` | 左摇杆 Y，`int8_t`，-100～100 |
| 7 | `LX` | 左摇杆 X，`int8_t`，-100～100 |
| 8 | `RY` | 右摇杆 Y，`int8_t`，-100～100 |
| 9 | `RX` | 右摇杆 X，`int8_t`，-100～100 |
| 10 | `SW` | 4 个两档拨杆状态，2 bit/个 |
| 11 | `BTN` | 4 个按键状态和校准有效标志 |
| 12～13 | `CRC_L CRC_H` | CRC-16/MODBUS，低字节在前 |

### CRC-16/MODBUS

- 初值：`0xFFFF`
- 多项式：`0xA001`
- 计算范围：字节偏移 `2～11`，共 10 字节
- 发送顺序：低字节、后高字节

只有帧头、版本、类型、长度及 CRC 全部正确，接收机才将其认定为“已解析有效帧”。

### SW 拨杆字段

| 拨杆 | SW 位 |
|---|---|
| 拨杆 1 | bit1:0 |
| 拨杆 2 | bit3:2 |
| 拨杆 3 | bit5:4 |
| 拨杆 4 | bit7:6 |

每个拨杆编码：

```text
01 = A 档
10 = B 档
00 = 保留/异常
11 = 无效/异常
```

安全状态下，`00` 与 `11` 都会拒绝控制输出。

### BTN 按键字段

```text
bit0 = 按键 1：0=关，1=开
bit1 = 按键 2：0=关，1=开
bit2 = 按键 3：0=关，1=开
bit3 = 按键 4：0=关，1=开
bit4 = 摇杆校准有效：必须为 1
bit5～7 = 保留：必须为 0
```

按键是**持续状态量**，而非单次触发事件。

### 安全测试帧

下列帧表示所有摇杆为 0、四个拨杆全为 A 档、四个按键都关、校准有效：

```text
A5 5A 01 01 00 06 00 00 00 00 55 10 74 CE
```

其中：`SW=0x55`，`BTN=0x10`，CRC 为 `0xCE74`，所以字节顺序为 `74 CE`。

---

## 3. 冗余链路与安全状态

链路优先级固定为：

```text
USART2 > USART1 > USART3
```

有效链路定义为：最近 **300 ms** 内至少收到一帧 CRC 正确的完整控制帧。

接收机进入安全状态的条件：

1. 上电后尚无在线有效链路，或超过 300 ms 未收到有效帧；
2. 当前按优先级选中的链路发生 CRC 错；
3. 当前帧任一拨杆为 `00` 或 `11`；
4. `BTN.bit4 == 0`，或 `BTN.bit5～7` 非 0；
5. 任一摇杆值超出 `-100～100`。

安全状态下：

```c
receiver_control_output.left_y  = 0;
receiver_control_output.left_x  = 0;
receiver_control_output.right_y = 0;
receiver_control_output.right_x = 0;
receiver_control_output.switches = 0;
receiver_control_output.buttons  = 0;
receiver_control_output.sequence = 0;
receiver_control_safe = 1;
```

当一个更高优先级链路在线但异常时，系统不会自动切换到低优先级链路；这是为了防止故障时控制源悄然切换。

---

## 4. 调试器：查看已解析的数据

所有数组第一个下标均为串口号：

```text
[0] = USART1 / PA10 RX
[1] = USART2 / PB4 RX
[2] = USART3 / PB11 RX
```

### 4.1 每路最近一帧（已完成 CRC 校验）

例如查看 USART1：

```c
receiver_control_last_frame_valid[0]
receiver_control_last_frame_safe[0]

receiver_control_last_frame[0].sequence
receiver_control_last_frame[0].left_y
receiver_control_last_frame[0].left_x
receiver_control_last_frame[0].right_y
receiver_control_last_frame[0].right_x
receiver_control_last_frame[0].switches
receiver_control_last_frame[0].buttons
```

含义：

- `receiver_control_last_frame_valid[x] == 1`：该路至少收到过一帧格式和 CRC 都正确的帧；
- `receiver_control_last_frame_safe[x] == 1`：该帧的摇杆、拨杆和校准位也满足安全要求；
- 最近帧会保留，便于在安全状态下诊断发送内容。

### 4.2 逐个查看拨杆和按键

不需要手工做位运算，直接观察：

```c
receiver_control_switch_state[0][0]  // USART1 拨杆1
receiver_control_switch_state[0][1]  // USART1 拨杆2
receiver_control_switch_state[0][2]  // USART1 拨杆3
receiver_control_switch_state[0][3]  // USART1 拨杆4

receiver_control_button_state[0][0]  // USART1 按键1
receiver_control_button_state[0][1]  // USART1 按键2
receiver_control_button_state[0][2]  // USART1 按键3
receiver_control_button_state[0][3]  // USART1 按键4

receiver_control_calibration_valid[0]
```

拨杆变量值：

```text
1 = A 档
2 = B 档
0 或 3 = 异常（会触发安全状态）
```

按键变量值：

```text
0 = 关
1 = 开
```

将第一维由 `[0]` 改为 `[1]` 或 `[2]`，即可查看 USART2 或 USART3。

### 4.3 当前最终控制输出

```c
receiver_control_output.left_y
receiver_control_output.left_x
receiver_control_output.right_y
receiver_control_output.right_x
receiver_control_output.switches
receiver_control_output.buttons
receiver_control_output.sequence

receiver_control_active_source
receiver_control_selected_source
receiver_control_safe
receiver_control_safe_reason
```

变量说明：

| 变量 | 说明 |
|---|---|
| `receiver_control_active_source` | 实际已经允许输出的串口编号；安全状态时为 `255` |
| `receiver_control_selected_source` | 按优先级选中的候选串口；即使它被安全检查拒绝，也会保留编号 |
| `receiver_control_safe` | `0=正常输出`，`1=安全状态` |
| `receiver_control_safe_reason` | 见下一表 |

`receiver_control_safe_reason`：

| 值 | 原因 |
|---:|---|
| 0 | 正常，允许输出 |
| 1 | 没有在线有效源，或有效帧已超时 |
| 2 | 当前选中链路发生 CRC 错 |
| 3 | 当前选中帧的拨杆、校准位、保留位或摇杆范围无效 |

> `receiver_control_output` 在安全状态会被清零。因此若要确认发送端实际发来的内容，请优先查看 `receiver_control_last_frame[x]` 和逐个解码的拨杆/按键变量。

---

## 5. UART 原始接收诊断

当“有串口数据但没有解析结果”时，依次观察：

```c
uart_rx_irq_count[0]
uart_rx_dma_event_count[0]
uart_rx_last_dma_size[0]
uart_rx_dma_byte_count[0]
uart_rx_dma_live_byte_count[0]

uart_rx_dma_start_ok_count[0]
uart_rx_dma_start_error_count[0]
uart_rx_last_dma_start_status[0]

uart_rx_error_callback_count[0]
uart_rx_last_error_code[0]
uart_rx_error_irq_count[0]
uart_rx_last_isr[0]
```

解释：

| 现象 | 判断 |
|---|---|
| `uart_rx_dma_start_error_count[x] > 0` | 该路 DMA 接收启动失败 |
| `uart_rx_dma_event_count[x]`、`uart_rx_dma_byte_count[x]` 都增加 | MCU 已收到了按 IDLE 分段的数据 |
| `uart_rx_dma_live_byte_count[x]` 增加，但 event 不增加 | 发送端连续发送，尚未形成 IDLE 间隔；DMA 已有数据 |
| 原始接收计数增加，但 `receiver_control_last_frame_valid[x] == 0` | 帧头、固定字段或 CRC 不正确 |
| `receiver_control_last_frame_valid[x] == 1`，但 `receiver_control_safe == 1` | 已解析，然而被安全规则拒绝；看 `receiver_control_safe_reason` 和逐个拨杆/按键变量 |

`uart_rx_irq_count[0]` 上电偶发出现一次 `1`，可能是 UART 初始化时的残留 IDLE/错误状态，并不能单独证明收到了有效字节。应以 DMA 字节计数、DMA 事件或已解析帧变量作为判断依据。

---

## 6. 接入实际执行机构

目前 `receiver_control.c` 中的两个函数是弱实现，默认不驱动外部硬件：

```c
void ReceiverControl_Apply(const ReceiverControl_Output *output)
{
    (void)output;
}

void ReceiverControl_ApplySafeState(void)
{
}
```

在其他业务源文件中实现同名函数，即可覆盖弱实现。例如：

```c
void ReceiverControl_Apply(const ReceiverControl_Output *output)
{
    /* 将 output->left_y、output->left_x 等写入实际电机/继电器控制接口。 */
    /* BTN 是持续状态：保持为 1 时不要每帧重复执行一次性动作。 */
}

void ReceiverControl_ApplySafeState(void)
{
    /* 将电机停止、继电器断开或写入其他预设安全状态。 */
}
```

---

## 7. 主要源文件

| 文件 | 作用 |
|---|---|
| `Core/Src/control_protocol.c` | 14 字节帧解析、CRC-16/MODBUS、字段合法性判断 |
| `Core/Inc/control_protocol.h` | 协议字段与 CRC 接口 |
| `Core/Src/receiver_control.c` | 三路独立解析、链路优先级、安全控制和调试变量 |
| `Core/Inc/receiver_control.h` | 输出结构、状态和诊断变量声明 |
| `Core/Src/uart_rx.c` | 三路 UART Receive-to-IDLE DMA 接收、错误恢复、底层诊断 |
| `Core/Src/main.c` | UART 初始化、主循环与字节分发 |
| `CONTROL_RECEIVER.md` | 接收机实现说明与补充说明 |

---

## 8. 构建

推荐使用已配置的接收机构建目录：

```bash
cmake --build build/Receiver-Debug
```

生成的可执行文件：

```text
build/Receiver-Debug/e34-jieshou
```
