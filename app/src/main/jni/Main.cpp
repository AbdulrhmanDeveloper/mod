#include <list>
#include <vector>
#include <cstring>
#include <pthread.h>
#include <thread>
#include <cstring>
#include <string>
#include <jni.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <dlfcn.h>
#include "Includes/Logger.h"
#include "Includes/obfuscate.h"
#include "Includes/Utils.hpp"
#include "Menu/Menu.hpp"
#include "Menu/Jni.hpp"
#include "Includes/Macros.h"

#include <dirent.h>          // لـ DIR و opendir/readdir
#include <sys/stat.h>        // لـ struct stat
#include <sys/system_properties.h> // لـ __system_property_get
#include <errno.h>           // لـ errno = ENOENT
#include <string.h>          // لـ strstr, strcpy, strlen
#include <string>            // لـ std::string

// Dobby hook framework
#include <dobby/dobby.h>

bool noDeath;
int scoreMul = 1, coinsMul = 1;

// الأزرار الجديدة (ظاهرة بس بدون فعالية حالياً)
bool showLongLines = false;
bool showPocketCircles = false;
bool autoPlay = false;
bool bypassDetection = false;
bool rootBypassActive = true;  // تلقائي ON دايماً

const char* blockedPaths[] = {
    "/su", "/system/bin/su", "/system/xbin/su", "/system/app/Superuser.apk",
    "/data/adb", "/data/adb/magisk", "/data/adb/modules", "/sbin/su",
    "/proc/mounts", "/proc/self/mountinfo", "/proc/self/maps", "/proc/self/attr/prev",
    "test-keys", "magisk", "zygisk", "shamiko", "root"
};
const int blockedCount = sizeof(blockedPaths) / sizeof(blockedPaths[0]);
struct MemPatches {
    MemoryPatch noDeath;
} gPatches;

// GetFeatureList – القائمة اللي هتظهر الأزرار
jobjectArray GetFeatureList(JNIEnv *env, jobject context) {
    jobjectArray ret;

    const char *features[] = {
        OBFUSCATE("Category_8 Ball Pool Hacks 🎱"),  // عنوان جميل فوق الأزرار
        OBFUSCATE("0_CheckBox_ Show Long Lines 📏"),
        OBFUSCATE("1_CheckBox_ Show Pocket Circles 🔴"),
        OBFUSCATE("2_CheckBox_ Auto Play 🎯"),
        OBFUSCATE("3_CheckBox_ test test?")
    };

    int Total_Feature = (sizeof features / sizeof features[0]);
    ret = (jobjectArray)
            env->NewObjectArray(Total_Feature, env->FindClass(OBFUSCATE("java/lang/String")),
                                env->NewStringUTF(""));

    for (int i = 0; i < Total_Feature; i++)
        env->SetObjectArrayElement(ret, i, env->NewStringUTF(features[i]));

    return (ret);
}

bool btnPressed = false;

void Changes(JNIEnv *env, jclass clazz, jobject obj,
             jint featNum, jstring featName,
             jint value, jlong Lvalue,
             jboolean boolean, jstring text) {

    switch (featNum) {

        case 0: // Show Long Lines
            showLongLines = boolean;
            break;

        case 1: // Show Pocket Circles
            showPocketCircles = boolean;
            break;

        case 2: // Auto Play
            autoPlay = boolean;
            break;

        case 3: // test
            bypassDetection = boolean;
            break;

        default:
            break;
    }
}

//CharacterPlayer
void (*StartInvcibility)(void *instance, float duration);

void (*old_Update)(void *instance);

void Update(void *instance) {
    if (instance != nullptr) {
        if (btnPressed) {
            StartInvcibility(instance, 30);
            btnPressed = false;
        }
    }
    return old_Update(instance);
}

// AddScore & AddCoins
void (*old_AddScore)(void *instance, int score);
void AddScore(void *instance, int score) {
    return old_AddScore(instance, score * scoreMul);
}

void (*old_AddCoins)(void *instance, int count);
void AddCoins(void *instance, int count) {
    return old_AddCoins(instance, count * coinsMul);
}

//Target lib here
#define targetLibName OBFUSCATE("libil2cpp.so")

ElfScanner g_il2cppELF;

int (*orig_access)(const char* path, int mode);
int hooked_access(const char* path, int mode) {
    if (rootBypassActive && path) {
        std::string p(path);
        for (int i = 0; i < blockedCount; ++i) {
            if (p.find(blockedPaths[i]) != std::string::npos) {
                LOGI("Blocked access: %s", path);
                return -1;  // ENOENT
            }
        }
    }
    return orig_access(path, mode);
}

int (*orig_stat)(const char* path, struct stat* buf);
int hooked_stat(const char* path, struct stat* buf) {
    if (rootBypassActive && path) {
        std::string p(path);
        for (int i = 0; i < blockedCount; ++i) {
            if (p.find(blockedPaths[i]) != std::string::npos) {
                LOGI("Blocked stat: %s", path);
                errno = ENOENT;
                return -1;
            }
        }
    }
    return orig_stat(path, buf);
}

FILE* (*orig_fopen)(const char* path, const char* mode);
FILE* hooked_fopen(const char* path, const char* mode) {
    if (rootBypassActive && path) {
        std::string p(path);
        for (int i = 0; i < blockedCount; ++i) {
            if (p.find(blockedPaths[i]) != std::string::npos) {
                LOGI("Blocked fopen: %s", path);
                errno = ENOENT;
                return nullptr;
            }
        }
    }
    return orig_fopen(path, mode);
}

DIR* (*orig_opendir)(const char* path);
DIR* hooked_opendir(const char* path) {
    if (rootBypassActive && path) {
        std::string p(path);
        for (int i = 0; i < blockedCount; ++i) {
            if (p.find(blockedPaths[i]) != std::string::npos) {
                LOGI("Blocked opendir: %s", path);
                errno = ENOENT;
                return nullptr;
            }
        }
    }
    return orig_opendir(path);
}

int (*orig_property_get)(const char* name, char* value, const char* default_value);
int hooked_property_get(const char* name, char* value, const char* default_value) {
    int res = orig_property_get(name, value, default_value);
    if (rootBypassActive && name && strstr(name, "ro.build.tags")) {
        strcpy(value, "release-keys");
        LOGI("Spoofed ro.build.tags to release-keys");
        return strlen(value);
    }
    return res;
}

void hack_thread() {
    LOGI(OBFUSCATE("pthread created"));

    while (!isLibraryLoaded(targetLibName)) {
        sleep(1);
    }

    do {
        sleep(1);
        g_il2cppELF = ElfScanner::createWithPath(targetLibName);
    } while (!g_il2cppELF.isValid());

    LOGI(OBFUSCATE("%s has been loaded"), (const char *) targetLibName);

#if defined(__aarch64__)
    uintptr_t il2cppBase = g_il2cppELF.base();

    StartInvcibility = (void (*)(void *, float)) getAbsoluteAddress(targetLibName, str2Offset(
            OBFUSCATE("0x107A3BC")));

    HOOK(targetLibName, str2Offset(OBFUSCATE("0x107A2E0")), AddScore, old_AddScore);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x107A2FC")), AddCoins, old_AddCoins);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x1078C44")), Update, old_Update);

    gPatches.noDeath = MemoryPatch::createWithHex(il2cppBase + str2Offset(OBFUSCATE("0x1079728")), "C0 03 5F D6");

#endif

    LOGI(OBFUSCATE("Done"));
}

__attribute__((constructor))
void lib_main() {
    // Hooks تلقائية لـ root bypass - تنفذ فور تحميل الـ mod
    DobbyHook((void*)access, (void*)hooked_access, (void**)&orig_access);
    DobbyHook((void*)stat, (void*)hooked_stat, (void**)&orig_stat);
    DobbyHook((void*)fopen, (void*)hooked_fopen, (void**)&orig_fopen);
    DobbyHook((void*)opendir, (void*)hooked_opendir, (void**)&orig_opendir);
    DobbyHook((void*)__system_property_get, (void*)hooked_property_get, (void**)&orig_property_get);

    LOGI("Root bypass hooks installed automatically - No root detection possible");

    // الـ thread اللي عندك للحاجات التانية
    std::thread(hack_thread).detach();
}
