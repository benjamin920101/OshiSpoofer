#include <pthread.h>
#include <dlfcn.h>
#include <memory.h>
#include <stdio.h>

#include "jni.h"
#include "jvmti.h"

typedef void (*JVM_MonitorNotify)(JNIEnv *env, jobject obj);

typedef jint (JNICALL * Jvm) (JavaVM**, jsize, jsize*);

#define UNUSED(x) (void)(x)

// 偽造的硬體識別碼
#define FAKE_CPU_ID         "BFEBFBFF000906EA"
#define FAKE_BASEBOARD_SN   "MB-123456789"
#define FAKE_SYSTEM_UUID    "550e8400-e29b-41d4-a716-446655440000"
#define FAKE_MEMORY_SN      "RAM-987654321"

// 儲存要攔截的方法 ID
static jmethodID g_cpu_getProcessorId = NULL;
static jmethodID g_baseboard_getSerialNumber = NULL;
static jmethodID g_computerSystem_getUuid = NULL;
static jmethodID g_memory_getSerialNumber = NULL;

static jvmtiEnv* g_jvmti = NULL;

void JNICALL Breakpoint(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method, jlocation location);
void JNICALL VMinit(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread);

JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM *vm, char *options, void *reserved) {
    UNUSED(options);
    UNUSED(reserved);
    jvmtiEnv* jvmti = NULL;

    (*vm)->GetEnv(vm, (void**)&jvmti, JVMTI_VERSION);
    g_jvmti = jvmti;

    jvmtiEventCallbacks events;
    jvmtiCapabilities capabilities;

    memset(&events, 0, sizeof(jvmtiEventCallbacks));
    memset(&capabilities, 0, sizeof(jvmtiCapabilities));

    events.Breakpoint = &Breakpoint;
    events.VMInit = &VMinit;

    capabilities.can_generate_breakpoint_events = 1;
    capabilities.can_force_early_return = 1;


    (*jvmti)->AddCapabilities(jvmti, &capabilities);
    (*jvmti)->SetEventCallbacks(jvmti, &events, sizeof(events));
    (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE,
                              JVMTI_EVENT_VM_INIT, (jthread)NULL);

    return 0;
}

void JNICALL Breakpoint(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method, jlocation location) {
    UNUSED(method);
    UNUSED(location);
    
    jstring fakeValue = NULL;
    
    // 根據被呼叫的方法返回對應的偽造值
    if (g_cpu_getProcessorId != NULL && method == g_cpu_getProcessorId) {
        fakeValue = (*jni_env)->NewStringUTF(jni_env, FAKE_CPU_ID);
    } else if (g_baseboard_getSerialNumber != NULL && method == g_baseboard_getSerialNumber) {
        fakeValue = (*jni_env)->NewStringUTF(jni_env, FAKE_BASEBOARD_SN);
    } else if (g_computerSystem_getUuid != NULL && method == g_computerSystem_getUuid) {
        fakeValue = (*jni_env)->NewStringUTF(jni_env, FAKE_SYSTEM_UUID);
    } else if (g_memory_getSerialNumber != NULL && method == g_memory_getSerialNumber) {
        fakeValue = (*jni_env)->NewStringUTF(jni_env, FAKE_MEMORY_SN);
    }
    
    if (fakeValue != NULL) {
        (*jvmti_env)->ForceEarlyReturnObject(jvmti_env, thread, fakeValue);
    }
}

void JNICALL VMinit(jvmtiEnv *jvmti, JNIEnv* env, jthread thread) {
    UNUSED(thread);
    
    // 1. 攔截 CPU ProcessorId (Linux 平台)
    jclass cpuClass = (*env)->FindClass(env, "oshi/driver/linux/proc/CpuInfo");
    if (cpuClass != NULL) {
        g_cpu_getProcessorId = (*env)->GetMethodID(env, cpuClass, "getProcessorID", "()Ljava/lang/String;");
        if (g_cpu_getProcessorId != NULL) {
            (*jvmti)->SetBreakpoint(jvmti, g_cpu_getProcessorId, 0);
        }
    }
    
    // 2. 攔截主板序列號 (Linux 平台)
    jclass baseBoard = (*env)->FindClass(env, "oshi/hardware/platform/linux/LinuxBaseboard");
    if (baseBoard != NULL) {
        g_baseboard_getSerialNumber = (*env)->GetMethodID(env, baseBoard, "getSerialNumber", "()Ljava/lang/String;");
        if (g_baseboard_getSerialNumber != NULL) {
            (*jvmti)->SetBreakpoint(jvmti, g_baseboard_getSerialNumber, 0);
        }
    }
    
    // 3. 攔截系統 UUID (Linux 平台)
    jclass computerSystem = (*env)->FindClass(env, "oshi/hardware/platform/linux/LinuxComputerSystem");
    if (computerSystem != NULL) {
        g_computerSystem_getUuid = (*env)->GetMethodID(env, computerSystem, "getHardwareUUID", "()Ljava/lang/String;");
        if (g_computerSystem_getUuid != NULL) {
            (*jvmti)->SetBreakpoint(jvmti, g_computerSystem_getUuid, 0);
        }
    }
    
    // 4. 攔截記憶體序列號 (Linux 平台)
    jclass memory = (*env)->FindClass(env, "oshi/hardware/platform/linux/LinuxPhysicalMemory");
    if (memory != NULL) {
        g_memory_getSerialNumber = (*env)->GetMethodID(env, memory, "getSerialNumber", "()Ljava/lang/String;");
        if (g_memory_getSerialNumber != NULL) {
            (*jvmti)->SetBreakpoint(jvmti, g_memory_getSerialNumber, 0);
        }
    }

    // 啟用斷點事件通知
    (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_BREAKPOINT, NULL);
}



