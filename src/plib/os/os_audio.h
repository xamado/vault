#ifndef OS_AUDIO_H
#define OS_AUDIO_H

#include <stdbool.h>

#define OS_AUDIO_OK 0
#define OS_AUDIO_ERR_UNKNOWN 1
#define OS_AUDIO_ERR_BUFFERLOST 2

#define OS_AUDIO_PLAY_LOOPING 1

typedef struct OSAudioDevice OSAudioDevice;
typedef struct OSAudioBuffer OSAudioBuffer;

typedef struct OSAudioWaveFormat {
    int channels;
    int bitsPerSample;
    int sampleRate;
} OSAudioWaveFormat;

typedef struct OSAudioBufferDesc {
    int bufferBytes;
    OSAudioWaveFormat format;
} OSAudioBufferDesc;

int os_audio_create_device(OSAudioDevice** device);
void os_audio_destroy_device(OSAudioDevice* device);

int os_audio_create_buffer(OSAudioDevice* device, const OSAudioBufferDesc* desc, OSAudioBuffer** buffer);
void os_audio_destroy_buffer(OSAudioBuffer* buffer);

int os_audio_buffer_play(OSAudioBuffer* buffer, int flags);
int os_audio_buffer_stop(OSAudioBuffer* buffer);
int os_audio_buffer_set_volume(OSAudioBuffer* buffer, int volume);
int os_audio_buffer_get_volume(OSAudioBuffer* buffer, int* volume);
int os_audio_buffer_get_current_position(OSAudioBuffer* buffer, unsigned int* playPos, unsigned int* writePos);
int os_audio_buffer_set_current_position(OSAudioBuffer* buffer, unsigned int playPos);
int os_audio_buffer_lock(OSAudioBuffer* buffer, unsigned int offset, unsigned int bytes, void** ptr1, unsigned int* bytes1, void** ptr2, unsigned int* bytes2);
int os_audio_buffer_unlock(OSAudioBuffer* buffer, void* ptr1, unsigned int bytes1, void* ptr2, unsigned int bytes2);
int os_audio_buffer_get_status(OSAudioBuffer* buffer, bool* is_playing, bool* is_looping);

#endif /* OS_AUDIO_H */
