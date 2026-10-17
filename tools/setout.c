// setout.c — 按名字把系统默认输出设备切到指定音频设备
// 用法: ./setout "Dream ADA-8"
#include <CoreAudio/CoreAudio.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <设备名子串>\n", argv[0]); return 2; }

    AudioObjectID ids[64]; UInt32 sz = sizeof(ids);
    AudioObjectPropertyAddress pa = {
        kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain };
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &pa, 0, NULL, &sz, ids))
        { fprintf(stderr, "✗ 枚举设备失败\n"); return 1; }

    int found = 0;
    for (UInt32 i = 0; i < sz / sizeof(AudioObjectID); i++) {
        CFStringRef cf = NULL; UInt32 nsz = sizeof(cf);
        AudioObjectPropertyAddress na = {
            kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain };
        if (AudioObjectGetPropertyData(ids[i], &na, 0, NULL, &nsz, &cf) || !cf) continue;
        char name[256];
        CFStringGetCString(cf, name, sizeof(name), kCFStringEncodingUTF8);
        CFRelease(cf);
        if (strstr(name, argv[1])) {
            AudioObjectPropertyAddress da = {
                kAudioHardwarePropertyDefaultOutputDevice,
                kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
            AudioObjectID id = ids[i];
            if (AudioObjectSetPropertyData(kAudioObjectSystemObject, &da, 0, NULL,
                                           sizeof(id), &id)) {
                fprintf(stderr, "✗ 切换失败（权限?）: %s\n", name); return 1;
            }
            printf("✓ 默认输出 → [%u] %s\n", id, name);
            found = 1;
            break;
        }
    }
    if (!found) { fprintf(stderr, "✗ 没找到匹配 \"%s\" 的设备\n", argv[1]); return 1; }
    return 0;
}
