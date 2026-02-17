# T-Display-S3 LCD 基本測試指南

## 目的

建立一個最簡單的 LCD 測試程式，用於驗證硬體和驅動是否正常工作。

## 測試內容

### 測試 1：單色全螢幕填充
依序顯示以下顏色，每個顏色持續 2 秒：
- 紅色 (RED)
- 綠色 (GREEN)
- 藍色 (BLUE)
- 白色 (WHITE)
- 黑色 (BLACK)

### 測試 2：圖案測試
依序顯示以下圖案，每個圖案持續 3 秒：
- 水平彩色條紋（6 種顏色）
- 垂直彩色條紋（6 種顏色）
- 黑白棋盤格（20x20 像素方格）
- 漸層（從黑到白）

## 如何使用

### 步驟 1：切換到測試模式

在您的 Windows 電腦上：

```powershell
# 進入專案目錄
cd C:\Projects\mimiclaw

# 備份原始 CMakeLists.txt
copy main\CMakeLists.txt main\CMakeLists.txt.backup

# 使用測試版本
copy main\CMakeLists.txt.test main\CMakeLists.txt
```

### 步驟 2：編譯並燒錄

```powershell
# 設定 ESP-IDF 環境
cd C:\Espressif\frameworks\esp-idf-v5.5.2
.\export.ps1

# 回到專案目錄
cd C:\Projects\mimiclaw

# 清理並重新編譯
idf.py fullclean
idf.py set-target esp32s3
idf.py build

# 燒錄
idf.py -p COM3 flash monitor
```

### 步驟 3：觀察結果

#### 如果螢幕正常：
- 您應該看到清晰的純色填充（紅、綠、藍、白、黑）
- 條紋圖案應該是清晰的水平或垂直線條
- 棋盤格應該是整齊的黑白方格
- 漸層應該是平滑的從黑到白過渡

#### 如果螢幕異常：
請記錄以下資訊：
1. **完全黑屏** - 可能是背光或電源問題
2. **完全白屏** - 可能是初始化問題
3. **花屏/雜訊** - 可能是時序或配置問題
4. **顏色錯誤** - 可能是 RGB 順序問題
5. **圖案變形** - 可能是解析度或座標問題

### 步驟 4：恢復原始程式

測試完成後，恢復原始 CMakeLists.txt：

```powershell
cd C:\Projects\mimiclaw
copy main\CMakeLists.txt.backup main\CMakeLists.txt
```

## 預期的 Monitor 輸出

```
========================================
  T-Display-S3 LCD Basic Test
========================================

I (xxx) mimi_test: Initializing LCD...
I (xxx) display_test: === T-Display-S3 LCD Test ===
I (xxx) display_test: Resolution: 170x320
I (xxx) display_test: Step 1: Initializing backlight (GPIO 38)
I (xxx) display_test: Backlight ON
I (xxx) display_test: Step 2: Resetting LCD (GPIO 5)
I (xxx) display_test: LCD reset complete
I (xxx) display_test: Step 3: Initializing I80 bus
I (xxx) display_test: I80 bus initialized
I (xxx) display_test: Step 4: Initializing LCD panel IO
I (xxx) display_test: LCD panel IO initialized
I (xxx) display_test: Step 5: Initializing ST7789 panel
I (xxx) display_test: ST7789 panel initialized
I (xxx) display_test: Step 6: Configuring ST7789
I (xxx) display_test: ST7789 configured
I (xxx) display_test: === LCD Test Initialization Complete ===
I (xxx) mimi_test: LCD initialized successfully!

I (xxx) display_test: === Starting LCD Test Sequence ===
I (xxx) display_test: Test 1: RED screen
I (xxx) display_test: Test 1: GREEN screen
I (xxx) display_test: Test 1: BLUE screen
...
```

## 故障排除

### 問題 1：編譯錯誤
確認您已經正確複製了 `CMakeLists.txt.test` 到 `CMakeLists.txt`

### 問題 2：燒錄失敗
- 檢查 USB 連接
- 確認 COM 埠號正確
- 嘗試按住 Boot 按鈕再插入 USB

### 問題 3：Monitor 沒有輸出
- 檢查 baud rate 是否為 115200
- 嘗試重新插拔 USB

## 測試檔案

- `main/display/display_test.c` - 測試程式實作
- `main/display/display_test.h` - 測試程式標頭檔
- `main/mimi_test.c` - 簡化的主程式
- `main/CMakeLists.txt.test` - 測試用編譯配置

## 下一步

根據測試結果：
- **如果測試通過** - 問題可能在四格佈局的實作邏輯
- **如果測試失敗** - 需要進一步檢查硬體配置和驅動參數
