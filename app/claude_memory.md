# Claude 工作记忆

> 用途：下次开发快速恢复上下文。包含已完成任务、未完成依赖、关键设计决策、文件结构。
> 更新策略：**仅在用户明确说"更新 claude_memory"时更新**，不要每次任务完成后自动更新。

---

## 项目背景

- **硬件**：GD32F470VET6（Cortex-M4，200MHz），板载 GD25Qxx SPI Flash、RS485、ADC、DAC、RTC、OLED、按键、SD 卡
- **目标**：实现上位机（PC）与下位机（MCU）的串口协议，通过 RS485（USART1, 19200 baud）通讯
- **协议格式**：帧结构以 **ASCII 字符串**形式收发（不是原始字节），CRC-16-Modbus 校验
- **任务下发顺序**：用户按 A-01、A-02、... 字母+数字顺序下发，每个验证通过后才下一个
- **约束文件**：`D:\ximenzi\competiton\confine.md`（含分层规范、复用规则、未知引脚必问规则、§4.1 通道数据上报必乘变比）

---

## 分层架构（遵 confine.md §4）

```
Driver/                  ─ 底层驱动（只读，除非用户授权）
  Components/bsp/        ─ BSP 引脚配置 + 外设初始化（B-05 / G-01 / J-01 后已有改动，详见 BSP 修改记录）
  Components/gd25qxx/    ─ SPI Flash 驱动（持久化用）
  Libraries/             ─ GD32 标准外设库
Protocol/                ─ 协议层（帧解析、组帧、CRC）
  proto_crc.h/.c
  proto_cmd.h            ─ 所有命令字 + 帧类型常量
  proto_frame.h/.c       ─ 解析器 + 组帧器 + IEEE 754 BE pack/unpack + 自动上报帧助手
  proto_dispatch.h/.c    ─ 分发器：解析→守卫→路由→调 Function→组应答
Function/
  APP/                   ─ 调度器、任务（uart_task 接入分发器 + 自动上报 + 告警 tick + 延迟动作）
    usart_app.c/.h       ─ rs_usart_send() (含 RS485 切换延时) + uart_task 主循环
  sysFunction/           ─ 业务模块（每类功能独立成模块）
    sys_device.c/.h      ─ 设备 ID、固件版本、波特率码、重启请求、上电心跳标志
    sys_time.c/.h        ─ RTC BCD ↔ UTC Unix 秒 + format_datetime / format_utc + 公共 BCD 转换
    sys_param.c/.h       ─ CH0/CH1 变比 + 阈值（默认 ratio=1.0f, thresh=21.59f），setter 自动持久化
    sys_sample.c/.h      ─ CH0/CH1 采样 + 电压换算 × 变比
    sys_dac.c/.h         ─ DAC 输出封装（dac_data_set + raw 缓存）
    sys_report.c/.h      ─ 自动上报状态机（间隔/启停/上次发送时刻）
    sys_alarm.c/.h       ─ 告警检测、Flash 记录、ASCII 推送（CH2 已留 TODO 接入点）
    sys_sleep.c/.h       ─ 深度睡眠管理 + RTC ALARM0 配置 + RTC_Alarm_IRQHandler
    sys_storage.c/.h     ─ SPI Flash 持久化（blob v3，含所有可持久字段）
    sys_baudrate.c/.h    ─ 波特率码 ↔ 实际 baud + 应用到 USART1（M-01 引入）
    led_ctrl.c/.h        ─ LED 闪烁（已有，未动）
USER/
  src/main.c             ─ 启动流程（已加 bsp_adc1_init + sys_storage_init + sys_alarm_init，注释 test_spi_flash）
  src/gd32f4xx_it.c      ─ 中断（只读，所有新增 IRQ 处理放在 sysFunction 利用弱符号覆盖）
MDK/Project.uvprojx      ─ 已加入所有新文件和 Include Path
```

---

## 已完成任务清单（A → N）

### A 系列：基础通讯
| 任务 | 命令字 | 说明 | 状态 |
|------|--------|------|------|
| A-01 | 0x05 + 0xFFFF（广播扫描）| 收到广播 → 回设备心跳帧 `05 8888` | ✅（A-03 覆盖原响应） |
| A-02 | 0x01 + 0x0101 | 设备重启，先回 OK 再 `NVIC_SystemReset()` | ✅ |
| A-03 | — | 上电/复位自动发心跳帧 `05 8888`（uart_task 首次调用） | ✅ |

### B 系列：查询类
| 任务 | 命令字 | Payload | 状态 |
|------|--------|---------|------|
| B-01 | 0x01 + 0x0104 | 4 字节固件版本（默认 `02 00 01 00` = 2.0.1.0） | ✅ |
| B-02 | 0x01 + 0x0106 | 4 字节 UTC Unix 秒（大端，从 RTC BCD 换算） | ✅ |
| B-03 | 0x01 + 0x0112 | 1 字节波特率映射码（默认 `0x13` = 19200） | ✅ |
| B-04 | 0x01 + 0x0201 | CH0 IEEE 754 BE，板载电位器 PC0（ADC0_CH10） | ✅ |
| B-05 | 0x01 + 0x0202 | CH1 IEEE 754 BE，DAC 回读 PC1（ADC1_CH11，PA4 跳线 PC1）| ✅（G-01 后完整生效） |
| B-06 | 0x01 + 0x0400 | 读 CH0+CH1 阈值（8 字节双 BE 浮点），默认 `21.59f` | ✅ |

### C 系列：设置时间
| 任务 | 命令字 | Payload | 状态 |
|------|--------|---------|------|
| C-01 | 0x01 + 0x0105 | 4 字节 UTC 秒大端 → 写 RTC，回 OK | ✅ |

### D 系列：变比设置（即时生效）
| 任务 | 命令字 | Payload | 状态 |
|------|--------|---------|------|
| D-01 | 0x01 + 0x0241 | 4 字节 IEEE 754 BE → CH0 变比，回 OK，持久化 | ✅ |
| D-02 | 0x01 + 0x0242 | 4 字节 IEEE 754 BE → CH1 变比，回 OK，持久化 | ✅ |

### E 系列：阈值读写
| 任务 | 命令字 | 说明 | 状态 |
|------|--------|------|------|
| E-01a/b | 0x01 + 0x0401 / 0x0411 | 读/写 CH0 单通道阈值，与 0x0400 中 CH0 一致 | ✅ |
| E-02a/b | 0x01 + 0x0402 / 0x0412 | 读/写 CH1 单通道阈值 | ✅ |

### F 系列：持久化（重启保留）
| 任务 | 字段 | 状态 |
|------|------|------|
| F-01 | CH0 变比（**搭建了完整持久化层 sys_storage**） | ✅ |
| F-02 | CH1 变比（F-01 基础设施自动覆盖） | ✅ |
| F-03 | CH0 阈值（同上） | ✅ |
| F-04 | CH1 阈值（同上） | ✅ |

### G 系列：DAC 联动
| 任务 | 命令字 | Payload | 状态 |
|------|--------|---------|------|
| G-01 | 0x01 + 0x0301 | 2 字节大端 raw（0x0000~0x0FFF）→ `dac_data_set()` → PA4 输出，回 OK；CH1 联动有效 | ✅ |

### H 系列：自动上报
| 任务 | 命令字 | 说明 | 状态 |
|------|--------|------|------|
| H-01a | 0x01 + 0x0261 | 1 字节间隔码（01=1s/02=3s/03=5s），回 OK，持久化 | ✅ |
| H-01b | 0x01 + 0x0302 | 启动自动上报，**首帧由命令应答返回**（12B = ts+ch0+ch1），后续 uart_task 周期发 | ✅ |
| H-01c | 0x01 + 0x0303 | 停止自动上报，回 OK | ✅ |
| H-02  | — | 自动上报期间，dispatch 入口守卫只放行 0x0303，其余命令静默丢弃 | ✅ |
| H-03  | — | 同 H-01c | ✅ |

### I 系列：告警
| 任务 | 命令字 | 说明 | 状态 |
|------|--------|------|------|
| I-01 | 0x01 + 0x0601 | 1 字节模式（01=主动/02=被动），持久化；ASCII 推送格式 `YYYY-MM-DD HH:MM:SS \| CHx \| th \| val\n` | ✅ |
| I-02 | — | 触发验证（电位器/DAC 超阈值→上升沿触发记录+推送） | ✅ |
| I-03 | — | PASSIVE 模式仅入 Flash 不推送，逻辑天然支持 | ✅ |
| I-04a | 0x01 + 0x0602 | 查询最近 10 条告警，**纯 ASCII 非帧**，倒序；无记录回 `empty\n` | ✅ |
| I-04b | 0x01 + 0x0603 | 清空告警记录，回 OK 帧 | ✅ |

### J 系列：深度睡眠
| 任务 | 命令字 | 说明 | 状态 |
|------|--------|------|------|
| J-01 | 0x01 + 0x03AA | 回 OK 帧 → RTC ALARM0 10s 后唤醒 → 回 `instrument wakeup`（纯 ASCII 非帧） | ✅ |

### K 系列：错误应答帧（统一 FF EEEE）
| 任务 | 触发条件 | 行为 | 状态 |
|------|----------|------|------|
| K-01a | parse 返回 `PROTO_ERR_CRC` | 解出 device_id 匹配本机/广播 → 回 `FF EEEE`；否则静默 | ✅ |
| K-01b | parse 返回 `PROTO_ERR_LEN_FIELD`（plen 字段与实际帧长不符）| 同 K-01a | ✅ |
| K-01c | parse OK 但 `frame_type ∉ {CMD=0x01, HB=0x05}` | 回 `FF EEEE` + DEBUG_USART 日志 | ✅ |
| K-01d | 起止标志错/太短/奇长/非法 hex | 静默丢弃（无法定位目标） | ✅ |
| K-02  | 各 CMD 内 payload_len 与命令预期不符 | 统一回 `FF EEEE`（替代原 `FF + orig_cmd`） | ✅ |
| K-03  | 帧合法但 cmd_word 未实现（如 0x0102/0x0501）| 回 `FF EEEE` + DEBUG_USART 日志 | ✅ |
| —     | 业务 setter 校验失败（如 RTC 设置失败 / interval 码非法 / alarm 模式码非法）| 保留 `FF + orig_cmd`（不属于长度/帧错） | ✅ |

**例外**：H-02 自动上报期间所有 parse 错误也静默丢弃，保护上报流。

### L 系列：设备 ID 读写 + 持久化
| 任务 | 命令字 | Payload | 状态 |
|------|--------|---------|------|
| L-01 | 0x01 + 0x01A1 | 2B 大端 ID（0x0001~0xFFFE 合法；0x0000/0xFFFF 非法→静默不应答）| ✅ |
| L-02a| 0x01 + 0x0111 | 无 payload，应答 2B 当前 device_id（常用广播 FFFF 下发） | ✅ |
| L-02b| — | 重启后 ID 持久化（链路已就绪：blob v3 已含 device_id，setter 自动持久化）| ✅ |

**注意点**：L-01 应答帧的 `device_id` 字段用**新 ID**（非旧 ID），proto_build_ok(new_id, ...)。

### M 系列：波特率读写 + 持久化
| 任务 | 命令字 | Payload | 状态 |
|------|--------|---------|------|
| M-01 | 0x01 + 0x01A2 | 1B 码（0x11=4800/0x12=9600/0x13=19200/0x14=115200）→ 持久化 + 回 OK + 延迟重启 | ✅ |
| M-02 | — | 重启后波特率持久化（链路在 M-01 一并完成） | ✅ |

**码非法或长度错** → 回 `FF EEEE`（统一 K-02/K-03 风格）。

### N 系列：OTA 升级全流程（APP ↔ BL 协同）

| 任务 | 命令字 | 执行端 | 行为 | 状态 |
|------|--------|--------|------|------|
| N-01 | 0x01 + 0x0501 | APP | 回 OK → uart_task 写 BL 参数页 `update_flag=FAST_UPGRADE` + `app_baud_code` → reset 进 BL | ✅ |
| N-02 | 0x01 + 0x0502 | **BL**（也兼容 APP fallback）| 收 bin 切片 → 100ms 空闲判结束 → 校验前 4B 魔术字 `5A A5 C3 3C` → OK / FF+0x0502 ERROR | ✅ |
| N-03 | 0x01 + 0x0503 | **BL** | 收命令 → 回 OK → 设 PENDING + size + crc → reset → BL 复用现有 PENDING 安装路径 → 跳新 APP1 | ✅ |

**核心样例**（device_id=0x0001，已 node 验证）：
- N-01 OK：`A5B600010205010102FFCCB4B6A5`
- N-02 OK：`A5B600010205020102FF88B4B6A5`
- N-02 ERROR：`A5B60001FF05020002F173B6A5`（FF + orig_cmd 0x0502 + 无 payload）
- N-03 OK：`A5B600010205030102FF74B5B6A5`

**N-02 实现位置**：
- APP 端备份：`Function/sysFunction/sys_ota.{c,h}`（状态机 IDLE → VERIFYING → RESP_PENDING）
  - `ota_uart_process_frame` 入口守卫：sys_ota_is_verifying 时抢占字节流
  - dispatch 0x0502 分支：`ota_uart_reset_state` + `sys_ota_request_verify`，不立即应答
- BL 端主战场：`BootLoader/Function/BootLoader/bl_core.c` → `bl_handle_fast_upgrade()`

**N-03 实现位置**：BL `bl_core.c` `bl_wait_execute_cmd`（滑窗 + proto_parse 识别 0x0503）+ `bl_handle_fast_upgrade` 末段。

**BL 路径分发（bootloader_run）**：
- `update_flag == PENDING` → 现有安装路径（验证 + 拷贝 + 校验 + 跳 APP1）
- `update_flag == FAST_UPGRADE` → `bl_handle_fast_upgrade()`（详见下方"BL 主流程"）
- 其他 → `bl_quiet_wait_then_return()` 静默 5s → 跳 APP1（"无升级请求无任何输出"规范）

**BL 主流程 `bl_handle_fast_upgrade()` 关键时序**：
1. 立刻清 update_flag → IDLE → commit_param（防异常重启循环）
2. **预擦下载区 16KB**（PC 端 ~2s 处理 OK 应答的空闲窗口，避免后续 erase 阻塞 bin 接收）
3. 打印 `using command to interrupt start Application\r\n` + 倒计时 10 行（每 1s 一行：`wait for start Application(10s)......` → `(1s)`）
4. 滑窗 ASCII + proto_parse 识别 0x0502 → 进 bin 接收（100ms 空闲超时判结束）
5. **bin 接收循环**：
   - 魔术字 OK → 回 OK(0x0502) → 跳出循环
   - 魔术字错 → 回 ERROR(0x0502) → **再擦 16KB** → silent wait 下一次 0x0502（30s 窗口，60s 总上限）
6. 等 5s 0x0503：收到 → 回 OK(0x0503) → 设 PENDING + size + crc → reset 进 PENDING 路径
7. 超时 → return → 跳 APP1（保留旧固件）

**N-02/N-03 顺序场景关键**（来自 3.csv 教训）：
- N-02 测评错误固件后**不能立即跳 APP**，要继续等下一次 0x0502，让 N-03 正确固件能被 BL 接住
- bin 接收前**不能**擦 128KB（耗时 2-6s 错过 PC 的 bin 字节窗口），必须预擦 + 重试时擦

### O 系列：OLED 显示规范（任务完成）

**要求**：双行
- 第 1 行（y=0）：队伍编号 `2026834095`
- 第 2 行（y=2）：
  - BL 阶段（路径任意） → `Bootloader`
  - APP + 自动上报（sys_report_is_enabled）→ `AutoSample`
  - 其余 APP 时刻 → `IDLE`

**实现**：
- APP `Function/APP/oled_app.c::oled_task`（scheduler 每 10ms 调）：第 1 行队伍号，第 2 行根据 `sys_report_is_enabled()` 显示
- APP `oled_show_bootloader_screen()`：N-01 reset 前由 uart_task 调用一次，作为 BL 启动到 OLED_Init 之间的过渡保险
- **BL 工程独立 OLED 驱动**：复制 APP 的 `oled.{c,h}` + `oledfont.h` 到 `BootLoader/Driver/OLED/`，`delay_ms` 宏映射为 `delay_1ms`；复制 `gd32f4xx_i2c.{c,h}` + `gd32f4xx_dma.{c,h}` 到 BL Libraries；BL `bsp_oled_init`（I2C0 + DMA0_CH6 + PB8/PB9）逐行抄 APP；BL `main.c` 启动后 `OLED_ShowStr(0, 0, "2026834095", 16)` + `(0, 2, "Bootloader", 16)`

### P 系列：LED 协同（任务完成）

| LED | 引脚 | 功能 | 行为 |
|---|---|---|---|
| **LED1** | PD8 | 系统状态指示 | scheduler 启动后每 500ms `LED1_TOGGLE` → 1s 周期闪烁；上电进 APP 即开始 |
| **LED2** | PD9 | 采集工作指示 | `sys_report_is_enabled()` ? `LED2_ON` : `LED2_OFF`（500ms 跟随） |
| LED3~6 | PD10~13 | 未指派 | OFF |

**实现**：`Function/sysFunction/led_ctrl.c::led_ctrl_task`（scheduler 每 500ms 调）。BL 阶段两个灯都不点亮，符合"BL 不做任何 UART 输出"对应的最小行为。

---

## 关键模块详解

### 持久化层 `sys_storage`（F-01 引入，多次扩展至 v7）

**Flash 地址**：`0x000000`，单 sector（4KB）；`test_spi_flash()` 已在 main.c 注释释放
**blob 当前为 v7**（magic = `0xCAFE5707`）：
```c
struct {
    uint32_t magic;                 // 0xCAFE5707（v7）
    uint16_t version;                // 7
    uint16_t device_id;
    uint8_t  baud_code;              // 19200=0x13 default
    uint8_t  report_intv_code;       // 1s=0x01 default
    uint8_t  alarm_mode;             // PASSIVE=0x02 default
    uint8_t  reserved;
    float    ratio_ch0, ratio_ch1;
    float    thresh_ch0, thresh_ch1;
    uint16_t dac_raw;                // v5 起加入（F-02 修复）
    uint16_t reserved2;
    uint32_t checksum;               // simple byte sum
} SysStorageBlob;
```

**magic 升级历史**（每次升级会让旧 blob 失效一次 → 启动恢复所有默认值，等于"出厂复位"）：
- v3 `CAFE5703`：初始（device_id + baud + ratio + thresh + report_intv + alarm_mode）
- v4 `CAFE5704`：清测评后 baud=115200 残留（结构不变）
- v5 `CAFE5705`：加 `dac_raw` 字段（F-02 修复）
- v6 `CAFE5706`：再次清测评残留
- v7 `CAFE5707`：再次清测评残留（**当前**）

**手动恢复出厂的快捷做法**：每次升 magic 一位 → 重新编译烧入 → 启动自动重置所有持久化字段。比赛尚未实现 0x0102 工厂复位命令（建议长期方案）。

**读写时机**：
- `sys_storage_init()` 在 main 中调（scheduler_init 之前）
- `sys_param`/`sys_device`/`sys_report`/`sys_alarm` 的 setter **末尾自动调** `sys_storage_save()`
- 加载阶段 `s_loading=1` 阻止 setter 回写

**扩展指南**：新增持久化字段
1. 加到 `SysStorageBlob`（升 magic / version 触发旧 blob 失效重写默认）
2. `sys_storage_save()` 填字段、`sys_storage_init()` 加载时调对应 setter
3. 对应 setter 末尾要自动 `sys_storage_save()`

### 告警记录 `sys_alarm`（I-01 引入）

**Flash 地址**：`0x001000`，单 sector（4KB），最多 170 条
**记录格式**（24 字节）：
```c
struct { uint32_t magic; uint32_t utc; uint8_t ch; uint8_t pad[3];
         float threshold; float value; uint32_t pad2; } AlarmFlashEntry;
```
**防抖**：每通道 `s_was_over[]` 上升沿触发
**ASCII 格式**：`YYYY-MM-DD HH:MM:SS | CHx | th | val\n`（**手工拼字符**，绕 microlib 限制）
**满处理**：自动擦 sector 1 重新从 0 开始

### 波特率切换 `sys_baudrate`（M-01 引入）

**设计原则**：BSP 只读（confine.md §1），BSP `bsp_rs_usart_init` 硬编码 19200 作默认；本模块在 BSP 初始化后用持久化 `baud_code` 覆盖。

**接口**：
- `sys_baudrate_code_to_value(code)` → 实际 baud 值（0 = 非法码）
- `sys_baudrate_apply(code)` → 切 USART1 baud；内部 `usart_disable` → `usart_baudrate_set` → 清 IDLE → `usart_enable`

**调用时机**（main.c）：
```
bsp_usart_all_init()   // 默认 19200
sys_storage_init()     // 加载 baud_code
sys_baudrate_apply(sys_device_get_baudrate_code())  // 切到持久化 baud
delay_ms(100)          // 让总线 idle 沉淀 + PC 切换跟上
scheduler_init()
```

**协议时序（M-01 切 baud）**：
1. dispatch 收 0x01A2 → 持久化 baud_code → 回 OK（用旧 baud）
2. uart_task 发完 OK → `sys_device_reboot_if_pending()` 
3. **复位前 `RS485_CS_SET(0)` 强制 DE 低**（避免 reset 期间 TX 翻转漏到总线，PC 端显示 `?`）
4. `NVIC_SystemReset()`
5. 重启 → bsp init 19200 → storage 加载 → baudrate_apply 切到新 baud → delay_ms(100)
6. uart_task 首次执行发 A-03 心跳（新 baud）

**PC 操作约定**：收到 OK 后必须主动切串口 baud，约 100ms 内完成；否则双向不通讯。

### 自动上报 `sys_report` + uart_task 周期触发

- `sys_report_should_send(now_ms)` 判断到时
- uart_task 周期（5ms）调用，到时通过 `proto_build_auto_report()` 组帧 + `rs_usart_send()`
- 启停状态运行时存（重启回 OFF），间隔码持久化

### 深度睡眠 `sys_sleep`（J-01 引入）

- RTC ALARM0（mask 屏蔽日期，只匹配 H/M/S）+ EXTI line 17 + NVIC RTC_Alarm_IRQn
- **RTC_Alarm_IRQHandler 定义在 sys_sleep.c**（弱符号覆盖，不动 it.c）
- `sys_sleep_request(seconds)` 标记 → uart_task 应答发完后 `sys_sleep_execute_if_pending()` 执行
- `bsp_enter_deepsleep` 内部已含唤醒后重初始化，回 control 后发 `instrument wakeup`

### BootLoader 工程当前状态（N 系列 + OLED 接入后）

**目录**：`D:\ximenzi\competiton\BootLoader`

**目录结构（已扩充）**：
```
BootLoader/
  USER/src/main.c          ─ systick + bsp_usart_init + bl_apply_persistent_baud
                              + bsp_oled_init + OLED_Init + OLED 双行显示 + bootloader_run
  Function/BootLoader/bl_core.c   ─ 三路径分发 + bl_handle_fast_upgrade + OLED 双行
  Function/Parameter/             ─ bl_partition.h / bl_param.h（与 app/common/ 同步）
  Driver/BSP/                     ─ bsp_usart_init (RS485 USART1, baud 19200)
                                     + bsp_oled_init (I2C0 + DMA0_CH6, PB8/PB9)
  Driver/USART/                   ─ my_printf
  Driver/OLED/                    ─ 复制自 APP，delay_ms 改 delay_1ms
  Driver/Libraries/Source/        ─ 含新增的 gd32f4xx_i2c.c + gd32f4xx_dma.c
  Protocol/                       ─ bl_crc32.c/.h + bl_package.h + 复制自 APP 的
                                     proto_crc.{c,h} + proto_frame.{c,h} + proto_cmd.h
```

**Flash 分区**（与 APP 共享 `bl_partition.h`）：
- BootLoader：`0x08000000` ~ `0x0800FFFF`（64KB）
- 参数区：`0x08010000` ~ `0x08010FFF`（4KB）
- APP1：`0x08011000` ~ `0x08030FFF`（128KB）
- APP backup：`0x08031000` ~ `0x08050FFF`（128KB）
- Download/APP2：`0x08051000` ~ `0x08070FFF`（128KB）

**bl_param_t 结构（扩展后）**：
```c
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t update_flag;       // IDLE / PENDING / FAILED / FAST_UPGRADE
    uint32_t app_size;
    uint32_t app_crc32;
    uint32_t app1_addr;
    uint32_t app2_addr;
    uint32_t update_counter;
    uint32_t fail_counter;
    uint32_t last_error;
    uint32_t log_write_index;
    uint32_t app_baud_code;     // N-全套：APP 持久化的 baud_code (0x11~0x14)
    uint32_t reserved[50];
    uint32_t param_crc32;
    uint32_t tail_magic;
} bl_param_t;
```

`update_flag` 取值：
- `BL_UPDATE_FLAG_IDLE       = 0x00000000`
- `BL_UPDATE_FLAG_PENDING    = 0xAA55AA55`
- `BL_UPDATE_FLAG_FAILED     = 0xDEAD0001`
- `BL_UPDATE_FLAG_FAST_UPGRADE = 0x55AA55AA`（N-01 触发）

**BL 启动序列**（main.c）：
1. `systick_config` + `bsp_usart_init`（默认 19200）
2. **`bl_apply_persistent_baud()`** — 读 `bl_param_t.app_baud_code` 切 USART 到 APP 持久化的 baud
3. `bsp_oled_init` + `OLED_Init` + `OLED_ShowStr` 双行（队伍号 + `Bootloader`）
4. `bootloader_run()` → 三路径分发

**BL 主流程三路径**：
- `update_flag == PENDING` → 现有完整安装路径（验证下载包 + 备份 APP1 + 拷贝 + CRC 校验 + 必要时回滚 + reset + 启动新 APP1）
- `update_flag == FAST_UPGRADE` → `bl_handle_fast_upgrade()`（详见 N 系列章节）
- 其他 → `bl_quiet_wait_then_return()` 静默 5s → 跳 APP1

**APP→BL "升级请求"标志传递机制**：
- APP 0x0501 dispatch：`proto_build_ok` + `ota_request_fast_upgrade_mark()`（仅置 pending 标志）+ `sys_device_request_reboot()`
- uart_task：rs_usart_send(OK) → `ota_apply_fast_upgrade_mark_if_pending()` 擦写 BL 参数页设 `update_flag=FAST_UPGRADE` + `app_baud_code=当前 baud_code` → 若 apply 成功调 `oled_show_bootloader_screen()` 刷 OLED → `sys_device_reboot_if_pending()` 重启
- `ota_apply_*` 返回 `true=刚刚应用 pending` / `false=无 pending`，让 A-02/M-01 等普通 reboot 不会误刷 OLED

**Driver 改动（已用户授权）**：
- `BootLoader/Driver/BSP/mcu_cimc_gd32f470vet6.c`：baudrate 115200 → 19200 + 新增 `bsp_oled_init` + 全局 `oled_cmd_buf`/`oled_data_buf`
- BSP 头新增 OLED 引脚宏（I2C0_OWN_ADDRESS7、OLED_PORT、OLED_DAT_PIN 等）

### DAC 联动（G-01）

```
0x0301 → sys_dac_set_raw(raw) → dac_data_set() → TIMER5 触发 → PA4
                                                              ↓ 跳线
                                                              PC1 → ADC1 → CH1 上报
```

---

## 关键设计决策与默认值

### 协议常量（proto_cmd.h）
- `PROTO_VERSION = 0x02`，`PROTO_DEV_BROADCAST = 0xFFFF`
- 帧类型：CMD=0x01, ACK=0x02, HB=0x05, ERR=0xFF
- 设备心跳 CMD=0x8888；广播扫描 CMD=0xFFFF

### 设备默认配置
- 设备 ID：`0x0001`
- 固件版本：`2.0.1.0`（4 字节 `02 00 01 00`）
- 波特率码：`0x13`（19200，与 BSP 一致）
- 报告间隔码：`0x01`（1s）
- 告警模式：`0x02`（PASSIVE）

### 采样配置
- `SYS_SAMPLE_ADC_VREF = 3.3f`（按 VDDA，未走 PC2 校准）
- `SYS_SAMPLE_ADC_FULL_SCALE = 4095.0f`
- `SYS_PARAM_RATIO_DEFAULT = 1.0f`
- `SYS_PARAM_THRESH_DEFAULT = 21.59f`
- 公式（confine.md §4.1）：`reported = (raw / 4095) × Vref × ratio`

### 通讯
- RS485 端口：USART1（PA2/PA3），CS 控制脚 PA1
- 接收：DMA + IDLE 中断 → `rs_rxbuffer[]` + `rs_rx_flag/len`
- 发送：`rs_usart_send()` 在 usart_app.c，**前后各 delay_us(50)** 保 RS485 切换稳定
- 主调度：`scheduler_run()` 每 5ms 调一次 `uart_task`
- 上电心跳：`uart_task` 首次调用时发，由 `sys_device_startup_hb_pending()` 控

### 延迟动作机制
- `sys_device_request_reboot()` + `sys_device_reboot_if_pending()` （A-02）
- `sys_sleep_request()` + `sys_sleep_execute_if_pending()` （J-01）
- 都在 uart_task 应答发完后检查执行，保证 OK 帧完整上线再动作

---

## BSP / USER 修改记录（用户已授权）

| 文件 | 修改 | 任务 |
|------|------|------|
| `Driver/Components/bsp/mcu_cimc_gd32f470vet6.h` | 加 CH1 引脚宏 + `bsp_adc1_init` / `bsp_adc1_read_ch1` 声明 | B-05 |
| `Driver/Components/bsp/mcu_cimc_gd32f470vet6.c` | `bsp_dac_init` 后追加 `bsp_adc1_init()` + `bsp_adc1_read_ch1()` | B-05 |
| `Driver/Components/bsp/mcu_cimc_gd32f470vet6.c` | `bsp_deepsleep_reinit_after_wakeup()` 增加 `bsp_adc1_init()` 调用 | J-01 |
| `USER/src/main.c` | 加 `bsp_adc1_init()` + `sys_storage_init()` + `sys_alarm_init()` 调用 | B-05 / F-01 / I-01 |
| `USER/src/main.c` | 注释 `test_spi_flash()` 释放 sector 0 | F-01 |
| `USER/src/main.c` | 增加 `#include "sys_storage.h"` `#include "sys_alarm.h"` | F-01 / I-01 |
| `Function/APP/usart_app.c` | `rs_usart_send` 前后加 `delay_us(50)` | CH1 告警丢字节修复 |
| `USER/src/main.c` | 加 `sys_baudrate_apply()` + `delay_ms(100)` 在 storage init 后 | M-01 / M-02 |
| `USER/src/main.c` | 增加 `#include "sys_baudrate.h"` `#include "sys_device.h"` | M-01 |
| `Function/sysFunction/sys_device.c` | `sys_device_reboot_if_pending` 在 `NVIC_SystemReset` 前 `RS485_CS_SET(0)` | M-01 `?` 杂波修复 |
| `Protocol/proto_dispatch.c` | 新增 `CMD_OTA_REQUEST` / `0x0501` 分支：无 payload 回 OK，发完后软重启进 BootLoader | N-01 |
| `Protocol/proto_dispatch.c` | N-01 分支增加 `ota_request_fast_upgrade_mark()` 调用 | N-全套 baud 同步 |
| `Protocol/proto_dispatch.c` | 新增 `CMD_OTA_PREPARE` / `0x0502` 分支：调 `ota_uart_reset_state` + `sys_ota_request_verify`，返回 0 | N-02 |
| `Function/APP/usart_app.c` | uart_task 加 `sys_ota_tick` + `sys_ota_take_pending_response` 钩子 + `ota_apply_fast_upgrade_mark_if_pending` + 真 apply 时调 `oled_show_bootloader_screen` | N-02 / N-01 / OLED |
| `Function/APP/oled_app.c` | `oled_task` 第 1 行队伍号、第 2 行 IDLE/AutoSample；新增 `oled_show_bootloader_screen()` | OLED |
| `Function/sysFunction/led_ctrl.c` | LED1 1s 闪烁、LED2 跟随 `sys_report_is_enabled()`、LED3~6 OFF | LED |
| `Function/sysFunction/sys_dac.c` | `sys_dac_set_raw` 末尾 `sys_storage_save()`；新增 `sys_dac_restore_raw` | F-02 |
| `Function/sysFunction/sys_alarm.c` | `sys_alarm_clear_all` 不再 `s_was_over[i] = 0` | I-04 |
| `Function/sysFunction/sys_storage.c` | blob 升至 v7（最后一次 v7），含 `dac_raw` 字段；setter/restore 同步 | F-02 + 多次清残留 |
| `Function/sysFunction/sys_ota.h/.c` | 新建：N-02 APP 端魔术字校验状态机 | N-02 |
| `Protocol/ota_uart/ota_uart.h` | 加 `ota_request_fast_upgrade_mark` / `ota_apply_fast_upgrade_mark_if_pending` | N-01 |
| `Protocol/ota_uart/ota_uart.c` | 实现两接口；写 BL 参数页时同时填 `app_baud_code` | N-全套 baud 同步 |
| `Protocol/ota_uart/ota_uart.c` | 入口守卫：`sys_ota_is_verifying` 时 feed sys_ota | N-02 |
| `common/bl_param.h` | `bl_param_t` 加 `app_baud_code` 字段（reserved[51] 拆 1+50）| N-全套 baud 同步 |
| `MDK/Project.uvprojx` | sysFunction 组加 `sys_ota.c`；Include Path / 文件组持续维护 | 持续 |

**未触碰**：`USER/src/gd32f4xx_it.c`、Libraries/（原有部分）、startup_*.s

### BootLoader 工程改动（用户已授权）

| 文件 | 修改 | 任务 |
|------|------|------|
| `Driver/BSP/mcu_cimc_gd32f470vet6.c` | baudrate 115200 → 19200；新增 `bsp_oled_init` 函数 + 全局 OLED 缓冲 | N-01 + OLED |
| `Driver/BSP/mcu_cimc_gd32f470vet6.h` | 新增 OLED I2C0 引脚宏 + `bsp_oled_init` 声明 | OLED |
| `Driver/OLED/oled.{c,h}` `oledfont.h` | 从 APP 复制；`perf_counter.h` → `systick.h`，`delay_ms` 宏映射 `delay_1ms` | OLED |
| `Driver/Libraries/Source/gd32f4xx_i2c.c` `gd32f4xx_dma.c` | 从 APP 复制 | OLED |
| `Driver/Libraries/Include/gd32f4xx_i2c.h` `gd32f4xx_dma.h` | 从 APP 复制 | OLED |
| `Protocol/proto_crc.{c,h}` `proto_frame.{c,h}` `proto_cmd.h` | 从 APP 复制（让 BL 能解析 0x0502/0x0503 协议帧）| N-02 / N-03 |
| `Function/Parameter/bl_param.h` | 同步加 `app_baud_code` 字段 | N-全套 baud 同步 |
| `Function/BootLoader/bl_core.c` | 三路径分发（PENDING / FAST_UPGRADE / 静默）；删除旧 `bl_receive_uart_package`；新增 `bl_handle_fast_upgrade` + `bl_wait_prepare_with_countdown` / `bl_wait_prepare_silent` / `bl_recv_bin_and_check_magic` / `bl_wait_execute_cmd` / `bl_rs485_send` / `bl_uart_read_byte_nb` / `bl_quiet_wait_then_return`；预擦 16KB 优化；OLED 双行 | N-01/02/03 |
| `USER/src/main.c` | 加 `bl_apply_persistent_baud()`；OLED 初始化 + 双行显示；`#include` 调整 | N-全套 + OLED |
| `MDK/Project.uvprojx` | Driver/Peripherals 加 i2c/dma；Driver/OLED 组；Protocol 组加 proto_*.c；Include Path 加 `..\Driver\OLED` | 持续 |

---

## ⚠️ 已知缺陷与待办

### 必须解决
- **CH2 (PT100 外部 ADC) 未接入**
  - `sys_alarm_tick` 已留 TODO 注释
  - `SYS_ALARM_CH2 = 2` 常量已定义，防抖数组 `s_was_over[3]` 大小已留好
  - 后续需补 `sys_sample_get_ch2()` + `sys_param_get_ch2_thresh()` + 解锁 sys_alarm_tick 中的 CH2 检查
  - 同步实现 0x0221 / 0x0403 / 0x0413 协议命令

### 协议未实现命令
| 命令字 | 说明 | 难度 |
|--------|------|------|
| 0x0102 | 工厂复位 | 简单（清 SPI Flash sector 0 + BL 参数页 + reboot） |
| 0x0103 | 查询设备信息 | 简单（预留字段） |
| 0x0221 | 查询 CH2 (PT100) | 依赖 CH2 接入 |
| 0x0403 | 读 CH2 阈值 | 依赖 CH2 |
| 0x0413 | 写 CH2 阈值 | 依赖 CH2 |

### 已知行为缺陷
1. **PC 上位机可能不显示纯 ASCII 应答**：0x0602（告警列表）、`instrument wakeup`、`empty` 等响应是纯 ASCII，若 PC 工具只解析帧会看不到。MCU 端实现正确（confine.md §2 要求）。
2. **自动上报 + 告警同时活跃**：两者都在 RS485 上发，可能交错；H-02 守卫不阻塞告警 ASCII。PC 端需能解析两种格式（帧 + 纯 ASCII）。
3. ~~**DAC 值不持久化**~~（**已修 F-02**，v5 起 blob 含 `dac_raw`，setter 自动持久化 + 启动 restore）
4. **告警时间戳依赖 RTC 准确**：未设过时间时（默认 2025-04-30 23:59:50）告警显示该时间。先发 0x0105 设时间。
5. **microlib 启用**：项目用 microlib，**严禁在 RS485 输出代码中用 `%04u` / `%.2f` 等带宽度/精度的格式说明符**。如需格式化浮点/带宽度整数，复用 `fmt_float_2dec`（sys_alarm.c）或 `put2` / `sys_time_format_*`（sys_time.c）。
6. **N-01 BL 初始化打印评测脚本限制**：BL `using command...` + 倒计时 1s/行，评测脚本只等 2s 拿不全 10 行 → N-01 固定 0.5/1.0（部分通过）。代码无法改善评测脚本逻辑。
7. **板载电位器位置影响 D-01/F-01**：CH0 = PC0 板载电位器。旋钮在 0 端时 raw=0，变比 × 0 = 0，D-01 / F-01 无法验证比值。**测评前必须把电位器旋到中间位置（raw ≈ 1000~3000，电压 0.8~2.4V）**。
8. **每次测评后 baud/device_id 残留**：M-01 / L-01 持久化生效，PC 工具用默认参数连不上。当前快捷修复 = 升 `STORAGE_MAGIC` 一位 + 重新烧入 → 启动自动重置所有字段。长期方案是实现 0x0102 工厂复位命令。

### 潜在风险
- 自动上报间隔最快 1s，若有大量数据需求需考虑帧丢弃策略
- Flash 告警区满时直接擦写丢失旧记录（170 条上限），无日志轮转
- `rs_usart_send` 是阻塞发送，长帧会占用 uart_task 周期；最大帧 ~50 字节 × 10 bit / 19200 ≈ 26ms，对 5ms 周期有影响但可接受

---

## 验证命令模板（node 计算 CRC）

```bash
node -e "
function crc16(d){let c=0xFFFF;for(const b of d){c^=b;for(let i=0;i<8;i++)c=(c&1)?(c>>>1)^0xA001:c>>>1;}return c&0xFFFF;}
function build(devId,type,cmd,payload){
  const b=[0xA5,0xB6,(devId>>8)&0xFF,devId&0xFF,type,(cmd>>8)&0xFF,cmd&0xFF,payload.length,0x02,...payload];
  const crc=crc16(b);b.push((crc>>8)&0xFF,crc&0xFF,0xB6,0xA5);
  return b.map(x=>x.toString(16).padStart(2,'0').toUpperCase()).join('');
}
console.log(build(0x0001, 0x01, 0xXXXX, []));
"
```

---

## 恢复工作的步骤

1. 读 `D:\ximenzi\competiton\confine.md`（约束，含 §4.1 通道数据上报规则）
2. 读本文件（工作状态）
3. 读 `app/Protocol/proto_dispatch.c`（按 A→B→C→D→E→G→H→I→J→N 顺序列出主要命令分支）
4. 等待用户发下一个任务（按字母+数字顺序）
5. 按 confine.md §6.1 顺序检索 → §6.3 顺序处理 → 实现
6. 用 node 脚本验 CRC → 改 Keil 工程文件（如有新源文件）→ **不要自动更新本备忘**
7. 用户明确说"更新 claude_memory"时才回头更新此文件

**下一步最可能的任务**：
- 0x0102 工厂复位命令（避免每次测评后手动升 magic）
- 调查"v7 烧入后 device_id 仍不是 0x0001"问题（用户上次中断在此，需先确认烧入版本 + 查询命令）
- 0x0103 查询设备信息、CH2 接入（PT100：0x0221/0x0403/0x0413）
- 综合测试 / 上位机联调

---

## 🔴 当前未决问题（停在此处）

**用户停下时的问题**：v7 烧入后用户反馈"设备 ID 还是错的，不是 0x0001"。

**已确认的事**：
- 代码逻辑正确：`sys_device.c::s_device_id` static 初始化为 `SYS_DEVICE_DEFAULT_ID = 0x0001`
- v7 升级链路：`sys_storage_init` 读旧 v6 blob → magic 不匹配 → else 分支 → `sys_storage_save()` 写入当前内存默认值（含 device_id=0x0001）
- APP 工程构建 0 Error 0 Warning

**待用户回复以继续诊断**：
1. 烧入的是哪个版本？是不是 v7（最新构建）？
2. 怎么查的 device_id？返回什么值？
3. 烧完是否完整断电重启？（v7 的 `sys_storage_init` 只在重启路径上执行）

**调查工具/线索**：
- 让用户发 `A5B6FFFF010111000201B0B6A5`（广播查 ID）看返回什么
- 检查 sys_storage_init 的 else 分支是否需要更强力的"显式重置内存"
- 或者怀疑 SPI Flash `spi_flash_sector_erase` / `spi_flash_buffer_write` 没成功（init 顺序：先 `bsp_gd25qxx_init` 才 `sys_storage_init`）

---

## 测评分数演进

| 测评 | 总分 | 主要失分项 |
|------|------|-----------|
| 1.csv（2026-06-07 09:58）| 32.5 | D-01 / F-01（电位器 0）、F-02（DAC 不持久化）、I-04（清告警重触发）、N-01 部分、N-02 / N-03（BL baud 不同步 + 协议未实现）|
| 2.csv（2026-06-07 10:45）| 37.5 | N-01 部分 0.5 + N-03 失败（N-02 错误固件后 BL 跳 APP，N-03 0x0502 被 APP 处理，0x0503 落 K-03 错误帧）|
| 3.csv（2026-06-07 11:04）| ~38 | N-01 部分 0.5 + N-03 失败（修了"BL 不跳 APP"但 `bl_recv_bin_and_check_magic` 擦 128KB 耗时 2-6s 错过 bin 字节窗口）|
| 4.csv（待测）| 预期 ≥ 39 | 仅 N-01 部分 0.5（评测脚本硬限）|

**已完成失分修复**：
- **D-01 / F-01**（2 分）：板载电位器手动旋到中间位置（硬件操作，非代码）
- **F-02**（1 分）：blob v5 加 `dac_raw` 字段；`sys_dac_set_raw` 末尾 `sys_storage_save()`；启动 `sys_dac_restore_raw`
- **I-04**（1 分）：`sys_alarm_clear_all` 不再 `s_was_over[i] = 0`，保留防抖状态避免下个 tick 重触发上升沿
- **N-01 / N-02 / N-03 baud 同步**（2.5 分）：`bl_param_t` 加 `app_baud_code`，APP N-01 写入，BL `main` 启动 `bl_apply_persistent_baud` 切 USART
- **N-02 错误固件后不跳 APP**（保 N-03 1 分）：`bl_handle_fast_upgrade` 内层 while 循环，magic 错回 ERROR 后 silent wait 下一次 0x0502，30s/60s 双层超时
- **N-03 擦除时机**（保 N-03 1 分）：擦除从 `bl_recv_bin_and_check_magic` 移到 `bl_handle_fast_upgrade` 入口 + 重试前；缩小到 16KB（4 pages，~400-800ms）避免错过 bin 字节

---

## 失分修复记忆点（避免下次重蹈覆辙）

1. **GD32F470 Flash page erase 用 `FMC_PE_EN` 模式**，4KB/page，每 page ~50-200ms。擦 128KB = 32 pages → 2-6s。N-03 收 bin 路径上**绝不能**直接擦 128KB。
2. **GD32F4 Flash 不能直接覆盖写**（bit 0→1 不可），所以"上次写过的下载区"重试时必须重新擦。
3. **BL 阶段 RS485 通讯期间不能调 `my_printf`**：会 `RS485_CS_SET(1)` 切到发送态，期间 PC 来的字节全丢。
4. **滑窗 ASCII + proto_parse**：固定 26 字符尝试解析，失败丢首字节滑动；BL 端 0x0502/0x0503 都这么识别。
5. **BL 启动后 OLED 显存"复位不丢"**：APP `oled_show_bootloader_screen()` 在 reset 前写"Bootloader"，BL 自己 OLED_Init 之前画面保留（避免黑屏）。
6. **每次测评后必清持久化**：M-01 改 baud + L-01 改 ID + dac_raw 残留，PC 工具默认配置连不上。

### 已知行为细节补充（M-01/02 后）
- **波特率切换前 PC 必须先收到 OK 帧才切**：MCU 用旧 baud 发 OK，发完才重启。
- **重启耗时**：bsp_init + storage_init + 100ms 沉淀 ≈ 0.5-1s（依 OLED/SD/外设初始化）。
- **失败兜底**：blob magic/version/checksum 任一校验失败 → 全部回默认（含 baud=19200）。

### 已知行为细节补充（N 系列完整态）
- **0x0501 应答帧**：`A5B600010205010102FFCCB4B6A5`（device_id=0x0001 默认）
- **0x0502 OK**：`A5B600010205020102FF88B4B6A5`；**0x0502 ERROR**：`A5B60001FF05020002F173B6A5`
- **0x0503 OK**：`A5B600010205030102FF74B5B6A5`
- **device_id 兼容**：BL 应答用 PC 发来的 device_id（rx.device_id），不依赖 SPI Flash 读 ID（BL 不能访问 SPI Flash）
- **BL 阶段不点 LED**：LED1/LED2 都在 APP 路径上更新，BL 静默
