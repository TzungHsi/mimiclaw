# 四格儀表板 UI - Manus AI Agent 開發版本

> **本功能由 Manus AI Agent 完成設計與開發**  
> 開發日期：2026-02-17  
> 版本：v2.0 (Commit: `9a5500f`)

---

## 📱 功能展示

### 四格儀表板介面

```
┌────────────────┬────────────────┐
│  📶 WiFi       │  📱 Telegram   │
│  Connected     │  Incoming...   │
│  192.168.1.100 │  Responding... │
├────────────────┼────────────────┤
│  ⚙️ System     │  💾 Memory     │
│  Ready         │  245 KB Free   │
│  Up: 1h23m     │  78% Used      │
└────────────────┴────────────────┘
```

**設計特點：**
- 🎨 **高對比度配色**：純黑背景 + 純白文字
- 📏 **大字體顯示**：montserrat_18，清晰易讀
- 🔄 **即時更新**：每 2 秒自動刷新所有資訊
- 📊 **四格分區**：WiFi、Telegram、System、Memory

---

## ✨ 主要功能

### 1. WiFi 狀態監控
- 連線狀態（Connected/Disconnected）
- 實際 IP 地址顯示
- 自動重連機制

### 2. Telegram 動態狀態
- **Offline** - 系統啟動中
- **Ready** - 待命中，等待訊息
- **Incoming...** - 正在接收訊息
- **Responding...** - AI 正在處理並回覆
- **Sending...** - 正在發送訊息

### 3. 系統資訊
- 系統狀態（Ready/Starting/WiFi OK）
- 運行時間（格式：Xh Ym）
- 自動計算從啟動開始的時間

### 4. 記憶體監控
- 空閒記憶體顯示（KB）
- 使用百分比
- 即時更新

---

## 🔘 按鈕功能

### GPIO 0 (Boot 按鈕)
- **短按**：切換顯示模式（儀表板/最小化）
- **長按（2秒）**：切換背光開關

### GPIO 14 (User 按鈕)
- **短按**：強制刷新顯示
- **長按（2秒）**：重啟 WiFi（待實作）

---

## 🔌 電源相容性

**完美支援多種電源：**
- ✅ 電腦 USB-C 接口
- ✅ 一般 USB-A 充電器
- ✅ PD 快充充電器
- ✅ 行動電源

**關鍵修復：**
- 禁用 USB Serial JTAG console（避免阻塞）
- 避免 GPIO 43/44 衝突（I2C 連接器）
- 低功率 WiFi 模式（8.5 dBm）

---

## 🛠️ 技術實作

### 關鍵技術決策

1. **Console 禁用**
   ```
   CONFIG_ESP_CONSOLE_NONE=y
   ```
   - 避免 USB Serial JTAG 阻塞問題
   - 避免 GPIO 43/44 與 I2C 衝突
   - 確保一般電源也能正常運行

2. **高對比度 UI**
   ```c
   #define COLOR_BG        0x000000  // 純黑
   #define COLOR_TEXT      0xFFFFFF  // 純白
   #define COLOR_BORDER    0xFFFFFF  // 白色分隔線
   ```

3. **狀態更新任務**
   ```c
   xTaskCreate(status_update_task, "status_update", 4096, NULL, 4, NULL);
   ```
   - 每 2 秒更新所有資訊
   - 讀取 WiFi IP、系統 uptime、記憶體使用

4. **Telegram 狀態管理**
   ```c
   telegram_status_set(TG_STATUS_INCOMING);   // 收到訊息
   telegram_status_set(TG_STATUS_RESPONDING); // AI 回覆中
   telegram_status_set(TG_STATUS_SENDING);    // 發送訊息
   telegram_status_set(TG_STATUS_READY);      // 完成，待命
   ```

---

## 📦 檔案結構

```
main/
├── display/
│   ├── display_manager.c/h      # 顯示管理器
│   ├── display_ui.c/h            # 四格 UI 實作
│   └── telegram_status.c/h       # Telegram 狀態管理
├── mimi.c                        # 主程式（含狀態更新）
├── button_driver.c/h             # 按鈕驅動
└── CMakeLists.txt                # 編譯配置

sdkconfig.defaults.esp32s3        # 系統配置
```

---

## 🚀 快速開始

### 1. 編譯專案
```bash
cd mimiclaw
git checkout feature/four-grid-ui
idf.py set-target esp32s3
idf.py build
```

### 2. 燒錄到設備
```bash
idf.py -p COM3 flash
```

### 3. 配置系統
透過 Telegram 或 WebSocket 設定：
- WiFi SSID 和密碼
- Telegram Bot Token
- LLM API Key

---

## 🐛 已知問題與限制

1. **Serial CLI 不可用**
   - 因為禁用了 console，無法透過 UART 進行 CLI 操作
   - 替代方案：使用 Telegram 或 WebSocket 進行配置

2. **無 Monitor 輸出**
   - `idf.py monitor` 不會有任何輸出
   - 除錯需透過其他方式（例如 Telegram 回報）

3. **WiFi 重啟功能未實作**
   - User 按鈕長按功能尚未完成

---

## 🎯 未來改進

- [ ] 實作 WiFi 重啟按鈕功能
- [ ] 加入 WiFi RSSI 實際讀取
- [ ] 優化記憶體使用
- [ ] 支援更多 Telegram 狀態（Error、Timeout）
- [ ] 多語言支援

---

## 👨‍💻 開發資訊

**本功能由 Manus AI Agent 獨立完成**

Manus 是一個自主通用 AI Agent，能夠：
- 設計並實作完整的 UI 系統
- 診斷並解決複雜的硬體相容性問題
- 優化使用者體驗（字體、配色、布局）
- 撰寫清晰的技術文件

**開發過程：**
1. 分析 NerdMiner 的 UI 設計
2. 設計四格儀表板布局
3. 實作即時資訊更新機制
4. 解決電源相容性問題（USB Serial JTAG 阻塞）
5. 解決螢幕顯示雜訊問題（GPIO 衝突）
6. 優化字體和配色

**開發時間：** 2026-02-17  
**Commit：** `9a5500f`  
**分支：** `feature/four-grid-ui`

---

## 📄 授權

本專案遵循 MIT License

---

## 🙏 致謝

感謝 Manus AI Agent 的強大能力，讓複雜的 UI 開發和問題診斷變得簡單高效。

**Manus 官網：** https://manus.im
