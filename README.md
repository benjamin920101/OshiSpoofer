# OSHI HWID 攔截與模擬指南 (Windows)

本文檔說明如何在 Windows 環境下分析和繞過基於 OSHI (Operating System and Hardware Information) 的硬體設備識別碼 (HWID) 驗證機制。

## 目錄

- [概述](#概述)
- [HWID 採集點分析](#hwid-採集點分析)
- [生成與驗證流程](#生成與驗證流程)
- [攔截原理](#攔截原理)
- [實作步驟](#實作步驟)
- [使用方法](#使用方法)
- [注意事項](#注意事項)

---

## 概述

OSHI 是一個跨平台的系統和硬體資訊庫，常用於軟體授權和設備綁定驗證。程序透過 OSHI 驅動採集硬體特徵生成唯一的機器碼，用於驗證用戶身份。本方案透過 JVMTI (Java Virtual Machine Tool Interface) 在 Java 層攔截 OSHI 的硬體資訊獲取方法，強制返回預定義的偽造值，從而繞過 HWID 驗證。

---

## HWID 採集點分析

程序透過 OSHI 驅動採集以下硬體特徵，這些特徵被用於生成唯一的機器碼：

### 1. CPU 資訊

| 項目 | 詳細資訊 |
|------|----------|
| **類名** | `oshi.driver.windows.wmi.Win32Processor` |
| **屬性** | `ProcessorId` |
| **來源** | 透過 WMI 查詢 `Win32_Processor` 獲得 |
| **WMI 查詢語句** | `SELECT ProcessorId FROM Win32_Processor` |
| **攔截目標** | `oshi.hardware.CentralProcessor$ProcessorIdentifier.getProcessorID()` |

### 2. 主板/系統資訊

| 項目 | 詳細資訊 |
|------|----------|
| **類名** | `oshi.hardware.platform.windows.WindowsComputerSystem` |
| **父類** | `oshi.hardware.common.AbstractComputerSystem` |
| **特徵** | 系統序號 (Serial Number) 或 UUID |
| **WMI 查詢語句** | `SELECT SerialNumber, UUID FROM Win32_ComputerSystemProduct` |
| **攔截目標** | `oshi.hardware.ComputerSystem.getSerialNumber()`<br>`oshi.hardware.ComputerSystem.getHardwareUUID()` |

### 3. 內存特徵

| 項目 | 詳細資訊 |
|------|----------|
| **類名** | `oshi.driver.windows.wmi.Win32PhysicalMemory` |
| **特徵** | 內存條序號 |
| **WMI 查詢語句** | `SELECT SerialNumber FROM Win32_PhysicalMemory` |
| **攔截目標** | `oshi.hardware.GlobalMemory.getPhysicalMemory()[].getSerialNumber()` |

---

## 生成與驗證流程

1. **特徵拼接**: 
   - 程序呼叫 OSHI 方法獲取上述字串 (CPU ID、系統序號、記憶體序號等)
   - 將所有特徵拼接成一個原始長字串

2. **Native 轉換**: 
   - 拼接後的字串會傳遞給 Native 方法 (例如 `skidonion.TuTZY.Il.1l1l`) 進行處理
   - 此步驟通常在 DLL 中執行

3. **哈希加密**: 
   - 在 Native 層 (DLL) 中執行哈希運算（通常為 MD5 或 SHA-256）
   - 生成最終的 32 位或 64 位 HWID

4. **JWT 封裝**: 
   - 生成的 HWID 會作為一個 Claim (例如 `{"hwid": "..."}`) 放入 JWT Token 中

5. **網路驗證**: 
   - 程序啟動時會將此 Token 發送到伺服器進行校驗
   - 若硬體資訊不匹配則觸發保護邏輯

---

## 攔截原理

### 為什麼選擇在 Java 層攔截？

1. **避免 Native 層複雜性**: Native 方法通常經過混淆和加密，逆向難度高
2. **精準控制**: 在 OSHI 返回數據前直接攔截，確保所有調用點都使用偽造數據
3. **跨版本兼容**: OSHI 的 API 相對穩定，比 Native 實現更容易适配

### 技術方案

使用 **JVMTI Breakpoint 事件** + **ForceEarlyReturnObject**:

1. **設置斷點**: 在目標方法的入口處設置 Breakpoint
2. **攔截調用**: 當方法被調用時，JVMTI 觸發 `VM_EVENT_BREAKPOINT` 事件
3. **強制返回**: 使用 `ForceEarlyReturnObject` 函數讓方法立即返回預定義的偽造值
4. **跳過執行**: 原始的硬體採集邏輯完全不會執行

---

## 實作步驟

### 1. 準備環境

- **作業系統**: Windows 10/11
- **JDK**: 建議使用與目標程序相同版本的 JDK (需要包含 `jvmti.h`)
- **編譯器**: MinGW 或 Visual Studio
- **目標程序**: 基於 OSHI 進行 HWID 驗證的 Java 應用

### 2. 編寫 JVMTI Agent

創建 `hwid_spoof.c`:

```c
#include <jvmti.h>
#include <stdio.h>
#include <string.h>

// 偽造的硬體識別碼 (可自訂)
#define FAKE_CPU_ID "BFEBFBFF000906EA"
#define FAKE_SYSTEM_SERIAL "5CD1234567"
#define FAKE_SYSTEM_UUID "12345678-1234-1234-1234-123456789ABC"
#define FAKE_MEMORY_SERIAL "12345678"

static jvmtiEnv *g_jvmti = NULL;

// 攔截 CPU ProcessorID
void intercept_cpu_id(jvmtiEnv *jvmti, JNIEnv *jni, jthread thread, jmethodID method) {
    jstring fake_id = (*jni)->NewStringUTF(jni, FAKE_CPU_ID);
    (*jvmti)->ForceEarlyReturnObject(jvmti, thread, fake_id);
}

// 攔截系統序號
void intercept_system_serial(jvmtiEnv *jvmti, JNIEnv *jni, jthread thread, jmethodID method) {
    jstring fake_serial = (*jni)->NewStringUTF(jni, FAKE_SYSTEM_SERIAL);
    (*jvmti)->ForceEarlyReturnObject(jvmti, thread, fake_serial);
}

// 攔截系統 UUID
void intercept_system_uuid(jvmtiEnv *jvmti, JNIEnv *jni, jthread thread, jmethodID method) {
    jstring fake_uuid = (*jni)->NewStringUTF(jni, FAKE_SYSTEM_UUID);
    (*jvmti)->ForceEarlyReturnObject(jvmti, thread, fake_uuid);
}

// 攔截記憶體序號
void intercept_memory_serial(jvmtiEnv *jvmti, JNIEnv *jni, jthread thread, jmethodID method) {
    jstring fake_serial = (*jni)->NewStringUTF(jni, FAKE_MEMORY_SERIAL);
    (*jvmti)->ForceEarlyReturnObject(jvmti, thread, fake_serial);
}

// Breakpoint 事件回調
void JNICALL BreakpointHandler(jvmtiEnv *jvmti, JNIEnv *jni, jthread thread, jmethodID method, jlocation location) {
    char *class_name = NULL;
    char *method_name = NULL;
    
    jclass klass;
    (*jvmti)->GetMethodDeclaringClass(jvmti, method, &klass);
    (*jvmti)->GetClassSignature(jvmti, klass, &class_name, NULL);
    (*jvmti)->GetMethodName(jvmti, method, &method_name, NULL, NULL);
    
    // 攔截 CPU ID
    if (strstr(class_name, "CentralProcessor") && strstr(method_name, "getProcessorID")) {
        intercept_cpu_id(jvmti, jni, thread, method);
    }
    // 攔截系統序號
    else if (strstr(class_name, "ComputerSystem") && strstr(method_name, "getSerialNumber")) {
        intercept_system_serial(jvmti, jni, thread, method);
    }
    // 攔截系統 UUID
    else if (strstr(class_name, "ComputerSystem") && strstr(method_name, "getHardwareUUID")) {
        intercept_system_uuid(jvmti, jni, thread, method);
    }
    // 攔截記憶體序號
    else if (strstr(class_name, "PhysicalMemory") && strstr(method_name, "getSerialNumber")) {
        intercept_memory_serial(jvmti, jni, thread, method);
    }
    
    if (class_name) (*jvmti)->Deallocate(jvmti, (unsigned char*)class_name);
    if (method_name) (*jvmti)->Deallocate(jvmti, (unsigned char*)method_name);
}

// Agent 初始化
JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM *vm, char *options, void *reserved) {
    jvmtiEnv *jvmti = NULL;
    jvmtiCapabilities caps = {0};
    jvmtiEventCallbacks callbacks = {0};
    
    (*vm)->GetEnv(vm, (void**)&jvmti, JVMTI_VERSION_1_0);
    g_jvmti = jvmti;
    
    // 啟用 Breakpoint 事件
    caps.can_generate_breakpoint_events = 1;
    (*jvmti)->AddCapabilities(jvmti, &caps);
    
    // 註冊回調
    callbacks.Breakpoint = BreakpointHandler;
    (*jvmti)->SetEventCallbacks(jvmti, &callbacks, sizeof(callbacks));
    (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_BREAKPOINT, NULL);
    
    printf("[HWID Spoofer] Agent loaded successfully\n");
    return JNI_OK;
}
```

### 3. 編譯為 DLL

**使用 MinGW:**
```bash
gcc -shared -o hwid_spoof.dll hwid_spoof.c -I"%JAVA_HOME%\include" -I"%JAVA_HOME%\include\win32"
```

**使用 Visual Studio:**
```bash
cl /LD hwid_spoof.c /I"%JAVA_HOME%\include" /I"%JAVA_HOME%\include\win32"
```

### 4. 部署與注入

將生成的 `hwid_spoof.dll` 放置於目標程序可訪問的目錄，然後透過以下方式注入:

```bash
java -agentpath:/path/to/hwid_spoof.dll -jar target_application.jar
```

或在快捷方式中添加:
```
-agentpath:C:\path\to\hwid_spoof.dll
```

---

## 使用方法

### 快速開始

1. **修改偽造值**: 編輯 `hwid_spoof.c` 中的四個常量:
   ```c
   #define FAKE_CPU_ID "你的偽造CPU ID"
   #define FAKE_SYSTEM_SERIAL "你的偽造系統序號"
   #define FAKE_SYSTEM_UUID "你的偽造系統 UUID"
   #define FAKE_MEMORY_SERIAL "你的偽造記憶體序號"
   ```

2. **重新編譯**: 
   ```bash
   gcc -shared -o hwid_spoof.dll hwid_spoof.c -I"%JAVA_HOME%\include" -I"%JAVA_HOME%\include\win32"
   ```

3. **啟動程序**:
   ```bash
   java -agentpath:C:\path\to\hwid_spoof.dll -jar your_app.jar
   ```

### 驗證攔截效果

在程序中添加日誌或使用调试工具確認返回值:

```java
System.out.println("CPU ID: " + processor.getProcessorID());
System.out.println("System Serial: " = system.getSerialNumber());
System.out.println("System UUID: " + system.getHardwareUUID());
```

預期輸出應為你在 `hwid_spoof.c` 中定義的偽造值。

---

## 注意事項

### ⚠️ 法律免責聲明

- 本文件僅供教育和研究目的使用
- 請確保你擁有目標程序的合法授權
- 繞過軟體保護機制可能違反當地法律和服務條款
- 作者不對任何濫用行為負責

### 技術限制

1. **JVM 兼容性**: 
   - 需要與目標程序使用相同或兼容的 JVM 版本
   - 某些 JVM 可能限制 Breakpoint 事件的使用

2. **混淆與加密**:
   - 如果目標程序對 OSHI 類進行了重度混淆，可能需要調整攔截的類名和方法名
   - 使用反編譯工具 (如 JD-GUI) 確認實際的類名和方法簽名

3. **多線程問題**:
   - Breakpoint 事件可能在多個線程中觸發，確保代碼線程安全

4. **性能影響**:
   - Breakpoint 事件會帶來一定的性能開銷
   - 對於高頻調用的方法，考慮使用其他攔截技術 (如 ASM 字节碼修改)

### 進階技巧

1. **動態配置**: 將偽造值存放在外部配置文件中，無需重新編譯
2. **隨機化**: 每次啟動生成隨機的偽造值，增加追蹤難度
3. **條件攔截**: 僅在特定條件下啟用攔截，降低被檢測風險
4. **多方法攔截**: 擴展攔截更多 OSHI 方法 (如 MAC 地址、磁碟序列號等)

---

## 參考資源

- [JVMTI 官方文檔](https://docs.oracle.com/javase/8/docs/platform/jvmti/jvmti.html)
- [OSHI GitHub](https://github.com/oshi/oshi)
- [WMI 查詢參考](https://docs.microsoft.com/en-us/windows/win32/wmisdk/wmi-start-page)

---

## 常見問題

**Q: 為什麼我的斷點沒有觸發？**
- 確認類名和方法名完全匹配 (包括大小寫)
- 檢查目標類是否被自定義類加載器加載
- 嘗試在 `Agent_OnLoad` 中延遲設置斷點

**Q: 如何獲取真實的硬體資訊格式？**
- 使用 OSHI 示例程序或编写測試代碼輸出真實值
- 參考 OSHI 源碼了解各欄位的格式要求

**Q: 攔截後程序仍然檢測到異常？**
- 可能還有其他硬體特徵未被攔截 (如 MAC 地址、磁碟 ID 等)
- 檢查是否有完整性校驗機制檢測 JVMTI Agent
- 確認偽造值的格式符合程序預期

---

*最後更新: 2024*
