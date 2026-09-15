# 三路冗余无线遥控接收机

本工程运行在 **STM32G431CBTx**，角色为遥控器的**接收机**。三个不同频段的 E22 LoRa 模块分别通过 USART1、USART2、USART3 向 MCU 发送相同的 14 字节控制帧。接收机对每一路独立解析、校验 CRC，并根据链路优先级和安全规则输出最终控制状态。

> 本工程不会把收到的控制帧自动映射到电机、继电器或 I2C 外设。实际执行机构应在 `ReceiverControl_Apply()` 与 `ReceiverControl_ApplySafeState()` 中接入，见下文“接入实际输出”。

---

## 1. UART 与无线模块接线

| 串口 | 模块 | MCU TX | MCU RX | AUX | M1 | M0 | 诊断下标 |
|---|---|---:|---:|---:|---:|---:|---:|
| USART1 | E22-400 | PA9 | PA10 | PA8 | PB15 | PB14 | `0` |
| USART2 | E22-900 | PB3 | PB4 | PB5 | PB6 | PB7 | `1` |
| USART3 | E22-230 | PB10 | PB11 | PB2 | PB1 | PB0 | `2` |

模块接线必须交叉：模块 TXD → MCU RX，模块 RXD ← MCU TX。这组 `M0/M1/AUX` 是本板确认值，**不要套急停工程 USART1（PB6/PB7）**。

CubeMX 初始串口仍是 **9600 8N1**，因为 E22 配置模式固定 9600。`App_Init` 会先写寄存器（`REG0=0xE6`：工作串口 115200 + 空中 38.4 kbps，`REG3=0x00` 透传），C1 成功的通道再把 MCU 切到 115200，然后才启动 DMA 收包。失败通道保持 9600，不阻止另外两路收包。

所有模块必须与 MCU **共地**。`M0/M1` 上电默认拉低（透传），配置时临时 `M0=0`、`M1=1`。无线两端的地址、信道、空速和串口参数必须一致。

> `PA9` 已用作 USART1_TX，`PA8` 已用作 E22-1 AUX，因此本工程不会初始化旧工程的 I2C2。

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
debug.output.left_y  = 0;
debug.output.left_x  = 0;
debug.output.right_y = 0;
debug.output.right_x = 0;
debug.output.switches = 0;
debug.output.buttons  = 0;
debug.output.sequence = 0;
debug.safe = 1;
```

当一个更高优先级链路在线但异常时，系统不会自动切换到低优先级链路；这是为了防止故障时控制源悄然切换。

---

## 4. 调试器：查看已解析的数据

调试器只需 Watch 一个全局变量 `debug`（`Core/Inc/debug.h`），展开即可看到输出、三路解析结果和 UART 诊断。

所有数组第一个下标均为串口号：

```text
[0] = USART1 / PA10 RX
[1] = USART2 / PB4 RX
[2] = USART3 / PB11 RX
```

### 4.1 每路最近一帧（已完成 CRC 校验）

例如查看 USART1：

```c
debug.link[0].last_frame_valid
debug.link[0].last_frame_safe

debug.link[0].last_frame.sequence
debug.link[0].last_frame.left_y
debug.link[0].last_frame.left_x
debug.link[0].last_frame.right_y
debug.link[0].last_frame.right_x
debug.link[0].last_frame.switches
debug.link[0].last_frame.buttons
```

含义：

- `debug.link[x].last_frame_valid == 1`：该路至少收到过一帧格式和 CRC 都正确的帧；
- `debug.link[x].last_frame_safe == 1`：该帧的摇杆、拨杆和校准位也满足安全要求；
- 最近帧会保留，便于在安全状态下诊断发送内容。

### 4.2 逐个查看拨杆和按键

不需要手工做位运算，直接观察：

```c
debug.link[0].switch_state[0]  // USART1 拨杆1
debug.link[0].switch_state[1]  // USART1 拨杆2
debug.link[0].switch_state[2]  // USART1 拨杆3
debug.link[0].switch_state[3]  // USART1 拨杆4

debug.link[0].button_state[0]  // USART1 按键1
debug.link[0].button_state[1]  // USART1 按键2
debug.link[0].button_state[2]  // USART1 按键3
debug.link[0].button_state[3]  // USART1 按键4

debug.link[0].calibration_valid
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
debug.output.left_y
debug.output.left_x
debug.output.right_y
debug.output.right_x
debug.output.switches
debug.output.buttons
debug.output.sequence

debug.active_source
debug.selected_source
debug.safe
debug.safe_reason
```

变量说明：

| 变量 | 说明 |
|---|---|
| `debug.active_source` | 实际已经允许输出的串口编号；安全状态时为 `255` |
| `debug.selected_source` | 按优先级选中的候选串口；即使它被安全检查拒绝，也会保留编号 |
| `debug.safe` | `0=正常输出`，`1=安全状态` |
| `debug.safe_reason` | 见下一表 |

`debug.safe_reason`：

| 值 | 原因 |
|---:|---|
| 0 | 正常，允许输出 |
| 1 | 没有在线有效源，或有效帧已超时 |
| 2 | 当前选中链路发生 CRC 错 |
| 3 | 当前选中帧的拨杆、校准位、保留位或摇杆范围无效 |

> `debug.output` 在安全状态会被清零。因此若要确认发送端实际发来的内容，请优先查看 `debug.link[x].last_frame` 和逐个解码的拨杆/按键变量。

---

## 5. UART 原始接收诊断

当“有串口数据但没有解析结果”时，依次观察：

```c
debug.uart[0].irq_count
debug.uart[0].dma_event_count
debug.uart[0].last_dma_size
debug.uart[0].dma_byte_count
debug.uart[0].dma_live_byte_count

debug.uart[0].dma_start_ok_count
debug.uart[0].dma_start_error_count
debug.uart[0].last_dma_start_status

debug.uart[0].error_callback_count
debug.uart[0].last_error_code
debug.uart[0].error_irq_count
debug.uart[0].last_isr
```

解释：

| 现象 | 判断 |
|---|---|
| `debug.uart[x].dma_start_error_count > 0` | 该路 DMA 接收启动失败 |
| `debug.uart[x].dma_event_count`、`debug.uart[x].dma_byte_count` 都增加 | MCU 已收到了按 IDLE 分段的数据 |
| `debug.uart[x].dma_live_byte_count` 增加，但 event 不增加 | 发送端连续发送，尚未形成 IDLE 间隔；DMA 已有数据 |
| 原始接收计数增加，但 `debug.link[x].last_frame_valid == 0` | 帧头、固定字段或 CRC 不正确 |
| `debug.link[x].last_frame_valid == 1`，但 `debug.safe == 1` | 已解析，然而被安全规则拒绝；看 `debug.safe_reason` 和逐个拨杆/按键变量 |

`debug.uart[0].irq_count` 上电偶发出现一次 `1`，可能是 UART 初始化时的残留 IDLE/错误状态，并不能单独证明收到了有效字节。应以 DMA 字节计数、DMA 事件或已解析帧变量作为判断依据。

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
| `Core/Src/receiver_control.c` | 三路独立解析、链路优先级、安全控制 |
| `Core/Inc/receiver_control.h` | 输出结构与控制接口 |
| `Core/Inc/debug.h`、`Core/Src/debug.c` | 全部公开调试状态，调试器 Watch `debug` |
| `Core/Src/uart_rx.c` | 三路 UART Receive-to-IDLE DMA 接收、错误恢复、底层诊断 |
| `Core/Src/main.c` | UART 初始化、主循环与字节分发 |
| `CONTROL_RECEIVER.md` | 接收机实现说明与补充说明 |

---

## 8. 构建

只使用 CMake preset 构建，产物只有一个 ELF：

```bash
cmake --preset Debug
cmake --build --preset Debug
```

生成的可执行文件：

```text
build/Debug/e34-jieshou.elf
```
