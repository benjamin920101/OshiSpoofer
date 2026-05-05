<img width="5595" height="1037" alt="mermaid-1777962594069" src="https://github.com/user-attachments/assets/20fb40fd-bfc1-477b-9e99-304f287032c3" />
PID 13644 分析报告
🧠 进程身份



项目
值




进程名
Minecraft 1.21.1 (Fabric)


启动器
HMCL 3.13.0.338


玩家
benjamin920103


JDK
Java HotSpot 21.0.10 (64-bit)


操作系统
Windows 10 (TW locale)


游戏路径
C:\Users\Library\Documents\jdk-21_windows-x64_bin\...\minecraft



🔴 关键问题：Render thread 卡住
code复制"Render thread" #1 [12004] cpu=22078ms elapsed=404s
  TIMED_WAITING (on object monitor)
  at java.lang.Object.wait0(Native Method)
  at java.io.PipedInputStream.read(PipedInputStream.java:332)
  - locked <0x0000000087f73e88>
  at skidonion.AhxTR.I.II1(Native Method)    ← 罪魁祸首
  at skidonion.AhxTR.ll.1(Native Method)
  at skidonion.AhxTR.___.___(Native Method)
  at me.hachimiclient.client.??StopCracking$Rz.<clinit>   ← Hachimi 客户端
Render thread 被 native 函数 skidonion.AhxTR.I.II1() 阻塞，卡在 PipedInputStream.read() 上，调用链最终来自 Hachimi 客户端 的反破解（StopCracking）模块。
💾 内存状态（G1GC）



区域
使用
总计




G1 Heap
190 MB
768 MB


Young Gen
32 MB (1 个 region)
-


Survivors
0 MB
-


Metaspace
89 MB / 90 MB committed
1.1 GB reserved



堆使用正常，无内存压力。
JVM 参数：

-XX:InitialHeapSize=128MB
-XX:MaxHeapSize=2048MB
-XX:MaxGCPauseMillis=50
-XX:+UseG1GC

🧵 线程一览



线程
状态
说明




Render thread #1
🔴 TIMED_WAITING
卡在 skidonion native


Reference Handler
RUNNABLE
正常 daemon


Finalizer
WAITING
正常 daemon


DFU cleaning thread
TIMED_WAITING
ModernFix mod


Yggdrasil Key Fetcher
TIMED_WAITING
正版验证


Java2D Disposer
WAITING
正常 AWT


AWT-Windows
RUNNABLE
正常窗口事件循环


AWT-EventQueue-0
WAITING
正常 AWT 事件队列


C2 Compiler
RUNNABLE
JIT 编译 (累计 14s CPU)


C1 Compiler
RUNNABLE
JIT 编译 (累计 2.9s CPU)


GC Threads (×8)
RUNNABLE
G1 并行 GC



🔍 根因定位
skidonion native 库在读取 Pipe 时永久阻塞，导致 Render thread 整个卡死。skidonion 来自 Hachimi 客户端 mod，其 StopCracking 模块在初始化时做了某种反作弊检测或校验操作，触发了 native 侧的长等待/死锁。
✅ 建议

暂时移除 Hachimi 客户端 mod，重启游戏看是否恢复正常
如果是 skidonion 版本问题，检查是否有更新
如果游戏已卡死，需手动终止进程：code复制taskkill /PID 13644 /F

好，现在来深入拆解 oshi 在这个进程里的完整生成链路。
oshi 生成过程细节分析
📦 类实例分布（class_histogram）
code复制实例数   字节大小   类名
  13     416 B    oshi.PlatformEnum              ← 单例缓存（系统平台枚举）
   5     120 B    oshi.driver.windows.wmi.Win32PhysicalMemory$PhysicalMemoryProperty
   1     120 B    oshi.jna.Struct$CloseablePerformanceInformation
   1     104 B    oshi.jna.Struct$CloseableSystemInfo
   4      96 B    oshi.hardware.CentralProcessor$ProcessorCache$Type  (CPU缓存类型枚举)
   2      80 B    oshi.driver.windows.wmi.Win32Processor$ProcessorIdProperty
   4      96 B    oshi.driver.windows.perfmon.PagingFile$PagingPercentProperty
   1      40 B    oshi.util.Memoizer$1            ← 缓存持有内部类
   2      32 B    oshi.hardware.CentralProcessor$ProcessorCache$Type[]
   2      32 B    [Loshi.driver.windows.wmi.Win32Processor$ProcessorIdProperty;
   2      48 B    [Loshi.driver.windows.perfmon.PagingFile$PagingPercentProperty;
   1      72 B    [Loshi.PlatformEnum;
   + 9 个 Lambda 捕获对象 (各 16 B)
🔗 oshi 初始化链路
code复制me.hachimiclient.client.??StopCracking$Rz.<clinit>
    ↓
class_310.handler$bbo000$hachimi$hookInit()   ← HMCL Fabric Mixin 注入点
    ↓
    可能触发 oshi.SystemInfo.<clinit>
    └── oshi.util.Memoizer 缓存首次访问
           ↓
    ┌─────────────────────────────────────────┐
    │  oshi.SystemInfo.getHardware()           │  ← Facade 入口
    │    ↓                                    │
    │  oshi.hardware.platform.windows.         │  ← Windows 平台实现
    │    WindowsComputerSystem                 │  ← hostname / deviceId
    │    ↓                                    │
    │  oshi.hardware.platform.windows.         │  ← 物理内存
    │    WindowsGlobalMemory                   │
    │    ├── JNA: GlobalMemoryStatusEx()       │  ← kernel32.dll
    │    └── WMI: Win32_PhysicalMemory         │
    │    ↓                                    │
    │  oshi.hardware.platform.windows.         │  ← CPU / Processor
    │    WindowsCentralProcessor               │
    │    ├── JNA: GetLogicalProcessorInfo()   │  ← kernel32.dll
    │    ├── WMI: Win32_Processor             │
    │    └── perfmon: Win32_PerfFormattedData  │
    │        _PerfOS_PagingFile               │
    │    ↓                                    │
    │  oshi.jna.Struct$CloseableSystemInfo     │  ← NativeSystemInfo (JNA)
    │    └── GetNativeSystemInfo()             │  ← kernel32.dll
    └─────────────────────────────────────────┘
🪟 Windows 系统调用对应



JNA / WMI / perfmon
底层 Windows API
获取数据




CloseablePerformanceInformation
kernel32!GlobalMemoryStatusEx
物理内存总量 / 可用


CloseableSystemInfo
kernel32!GetNativeSystemInfo
CPU核心数 / 架构


LogicalProcessorInformation
kernel32!GetLogicalProcessorInformation
CPU缓存层级 (L1/L2/L3)


Win32_PhysicalMemory (WMI)
root\WMI → MS_SystemInformation
内存条 SMBIOS 信息


Win32_Processor (WMI)
root\WMI → 需额外查询
CPU ID / 型号 / 核心数


PagingFile (perfmon)
perfmon PDH
分页文件使用率



⚙️ oshi 的 lazy-init 机制
oshi.SystemInfo 使用 Memoizer 实现单例缓存：
code复制oshi.util.Memoizer<R, V>：
  - 内部持有 ConcurrentHashMap<R, V>
  - 首次调用 get(key, factory) 时执行 factory
  - 后续直接返回缓存值（不再发 Windows API）
这意味着 oshi 只在首次访问时 真正调用系统 API，后续重复调用都是纯内存读。
🔍 当前进程状态：oshi 已完成初始化
根据 class_histogram，所有 oshi 核心对象都已 实例化完毕：

PlatformEnum ×13 → 枚举常量池（按 CPU 核心分组）
GlobalMemory → CloseablePerformanceInformation 已实例化 → 持有 native 内存结构
SystemInfo → CloseableSystemInfo 已实例化 → 持有 native 系统信息
ProcessorCache 枚举 → CPU 缓存类型（L1 instruction / L1 data / L2 / L3）
Memoizer$1 → 缓存协调器实例

oshi 在 Render thread 启动过程中就已完成初始化，从 class_histogram 可见没有 pending 的 oshi worker thread 或未完成的 lazy 初始化。
🧵 注意：Timer hack thread 与 oshi 无关
java复制"Timer hack thread" #45 elapsed=488s
  TIMED_WAITING (sleeping)
  at java.lang.Thread.sleep0(Native Method)
  at net.minecraft.class_156$9.run(class_156.java:913)
这个线程是 Minecraft 内置的心跳线程（class_156 = MinecraftClient），定时 Thread.sleep() 维持帧率，与 oshi 完全无关。
📊 总结
oshi 在这个 Minecraft 进程里的生成路径：

触发时机：Render thread 初始化 → Hachimi Mixin hook → ??StopCracking$Rz.<clinit> → 可能调用 oshi 读取系统指纹
数据采集：通过 JNA + WMI + PDH perfmon 三路并行走 Windows Native API
缓存：用 Memoizer 包装，单次查询后完全缓存
当前状态：已完成初始化，所有结构体和枚举常量都在堆里，无 pending 线程，不造成额外 CPU 负担

