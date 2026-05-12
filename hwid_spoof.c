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
