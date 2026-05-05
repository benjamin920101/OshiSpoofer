```markdown
# HWID 與 驗證資訊提取報告 (Heap Dump 分析)

> ⚠️ **免責聲明**：本報告僅供安全研究、除錯與學術分析使用。請勿將提取之敏感資訊用於非法用途。所有資料皆來自本地堆轉儲檔案，分析過程符合合理使用原則。

---

## 1. 目標檔案資訊
| 項目 | 內容 |
|------|------|
| **檔案名稱** | `minecraft_hwid_heap_13644.hprof` |
| **檔案大小** | 281 MB |
| **分析環境** | Linux (Bash) / Windows (PowerShell 模擬) |
| **關聯軟體** | HMCL 3.13.0.338, Fabric Loader 0.18.6 |
| **關聯 Mod** | `hachimi-1.3.5-skidonion-release.jar` |
| **分析工具** | `strings`, `grep`, `hexdump`, JMAT, jhat |

---


---

## 3. 關鍵標識符與 HWID 候選值

### 🔹 A. 關鍵 SHA-256 雜湊值 (高度懷疑為 HWID)
```text
8D722F81A9C113C0791DF136A2966DB26C950A971DB46B4199F4EA54B78BFB9F
```
- **上下文**：出現在 `me/hachimiclient/client/StopCracking` 相關類別中
- **用途推測**：硬體指紋雜湊，用於綁定授權或防作弊驗證

### 🔹 B. 特殊 UUID 序列
以下 UUID 於 OSHI / WMI 相關緩存中被發現：

| UUID | 可能用途 |
|------|----------|
| `D8499B04-0E66-4726-AB29-64469D734E0D` | 系統主板/機型標識 |
| `3ceb37c0-db62-46b5-bd02-785457b01d96` | 磁碟/網路介面標識 |
| `FA233E1C-4180-4865-B01B-BCCE9785ACA3` | BIOS/固件標識 |
| `B9766B59-9566-4402-BC1F-2EE2A276D836` | 使用者會話/裝置標識 |

---

## 4. SkidOnion 驗證邏輯分析

該 Mod (`hachimi`) 整合 `skidonion.tech` 第三方驗證系統：

### 🔹 驗證端點
```text
https://skidonion.tech/
```

### 🔹 關鍵驗證欄位
| 欄位 | 說明 |
|------|------|
| `software_id` | 軟體唯一標識符 |
| `uid` | 使用者唯一 ID |
| `verify-token` | 一次性驗證令牌 |
| `jwt` | JSON Web Token 認證標誌 |
| `uin` | 疑似關聯 QQ 帳號之數值標識 |

### 🔹 混淆類別參考
- `skidonion.AhxTR.I`
- `skidonion.AhxTR.___`
- `me/hachimiclient/client/StopCracking`

---

## 5. 提取腳本與方法

### 🔸 方法一：快速提取所有 UUID
```bash
strings minecraft_hwid_heap_13644.hprof | \
  grep -oE "[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}" | \
  sort | uniq -c | sort -nr
```

### 🔸 方法二：搜索 OSHI 硬體屬性
```bash
strings minecraft_hwid_heap_13644.hprof | \
  grep -iE "ProcessorID|SerialNumber|IdentifyingNumber"
```

### 🔸 方法三：提取 SkidOnion 驗證上下文
```bash
strings minecraft_hwid_heap_13644.hprof | grep -C 10 "software_id"
```

### 🔸 方法四：定位 HWID 相關方法呼叫
```bash
strings minecraft_hwid_heap_13644.hprof | \
  grep -iE "getHardwareUUID|getHWID|getMachineId"
```

### 🔸 方法五：二進位偏移定位 (進階)
```bash
# 1. 取得字串偏移量
strings -t d minecraft_hwid_heap_13644.hprof | grep "ProcessorID:"

# 2. 假設偏移量為 4755522，查看原始數據
hexdump -C -s 4755522 -n 64 minecraft_hwid_heap_13644.hprof
```

---

## 6. 分析結論
1. 堆轉儲中確實存在多組硬體標識候選值，其中 SHA-256 雜湊 `8D72...FB9F` 最具備 HWID 特徵。
2. SkidOnion 驗證系統採用多欄位組合驗證（`software_id` + `uid` + `jwt`），並可能關聯第三方帳號 (`uin`)。
3. 建議進一步使用 Java Heap 分析工具（如 Eclipse MAT）對 `StopCracking` 類別進行物件關係追蹤，以確認 HWID 生成邏輯。

---

## 📋 更新記錄
| 版本 | 日期 | 說明 |
|------|------|------|
| v1.0 | 2026-05-05 | 初始報告建立，完成關鍵資訊提取與腳本整理 |

> 📌 **備註**：敏感資訊（如 Access Token）於公開分享前請務必脫敏處理。
```

✅ 檔案已格式化完成，可直接複製存為 `README.md`。  
