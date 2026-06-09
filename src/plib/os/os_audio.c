#include "os_audio.h"

#include <SDL2/SDL.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MASTER_FREQ 22050
#define MASTER_FORMAT AUDIO_S16SYS
#define MASTER_CHANNELS 2

struct OSAudioBuffer {
    uint8_t* dummy_data;
    int bufferBytes;
    OSAudioWaveFormat format;

    SDL_AudioStream* stream;
    bool playing;
    bool looping;
    int volume; // 0 to SDL_MIX_MAXVOLUME

    double playCursor;
    int readCursor;

    struct OSAudioDevice* device;

    struct OSAudioBuffer* next;
    struct OSAudioBuffer* prev;
};

struct OSAudioDevice {
    SDL_AudioDeviceID dev;
    SDL_mutex* mutex;
    OSAudioBuffer* buffers;
};

static void audio_callback(void* userdata, Uint8* stream, int len) {
    memset(stream, 0, len);
    OSAudioDevice* dev = (OSAudioDevice*)userdata;

    if (dev == NULL) return;

    SDL_LockMutex(dev->mutex);

    OSAudioBuffer* buf = dev->buffers;
    while (buf != NULL) {
        if (!buf->playing) {
            buf = buf->next;
            continue;
        }

        // Keep the stream buffered with enough data
        while (SDL_AudioStreamAvailable(buf->stream) < len * 2 && buf->playing) {
            int chunk = buf->bufferBytes - buf->readCursor;
            if (chunk > 1024) chunk = 1024;
            
            if (!buf->looping) {
                if (buf->readCursor >= buf->bufferBytes) {
                    break;
                }
            }

            SDL_AudioStreamPut(buf->stream, buf->dummy_data + buf->readCursor, chunk);
            buf->readCursor += chunk;

            if (buf->readCursor >= buf->bufferBytes) {
                if (buf->looping) {
                    buf->readCursor = 0;
                } else {
                    buf->readCursor = buf->bufferBytes;
                }
            }
        }

        int avail = SDL_AudioStreamAvailable(buf->stream);
        int pull_len = (avail < len) ? avail : len;
        
        if (pull_len > 0) {
            Uint8 temp[pull_len];
            int pulled = SDL_AudioStreamGet(buf->stream, temp, pull_len);
            if (pulled > 0) {
                SDL_MixAudioFormat(stream, temp, MASTER_FORMAT, pulled, buf->volume);
                
                double src_bytes_per_sec = (double)(buf->format.sampleRate * buf->format.channels * (buf->format.bitsPerSample / 8));
                double dest_bytes_per_sec = (double)(MASTER_FREQ * MASTER_CHANNELS * 2);
                double ratio = src_bytes_per_sec / dest_bytes_per_sec;
                
                buf->playCursor += pulled * ratio;
                if (buf->playCursor >= buf->bufferBytes) {
                    if (buf->looping) {
                        buf->playCursor -= buf->bufferBytes;
                    } else {
                        buf->playing = false;
                    }
                }
            }
        } else if (!buf->looping && buf->readCursor >= buf->bufferBytes) {
            buf->playing = false;
        }

        buf = buf->next;
    }

    SDL_UnlockMutex(dev->mutex);
}

int os_audio_create_device(OSAudioDevice** device) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        return OS_AUDIO_ERR_UNKNOWN;
    }

    OSAudioDevice* dev = (OSAudioDevice*)malloc(sizeof(OSAudioDevice));
    dev->mutex = SDL_CreateMutex();
    dev->buffers = NULL;

    SDL_AudioSpec desired, obtained;
    SDL_zero(desired);
    desired.freq = MASTER_FREQ;
    desired.format = MASTER_FORMAT;
    desired.channels = MASTER_CHANNELS;
    desired.samples = 1024;
    desired.callback = audio_callback;
    desired.userdata = dev;

    dev->dev = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, 0);
    if (dev->dev == 0) {
        SDL_DestroyMutex(dev->mutex);
        free(dev);
        return OS_AUDIO_ERR_UNKNOWN;
    }

    SDL_PauseAudioDevice(dev->dev, 0);

    *device = dev;
    return OS_AUDIO_OK;
}

void os_audio_destroy_device(OSAudioDevice* device) {
    if (device == NULL) return;
    SDL_CloseAudioDevice(device->dev);
    SDL_DestroyMutex(device->mutex);
    free(device);
}

int os_audio_create_buffer(OSAudioDevice* device, const OSAudioBufferDesc* desc, OSAudioBuffer** buffer) {
    OSAudioBuffer* buf = (OSAudioBuffer*)malloc(sizeof(OSAudioBuffer));
    memset(buf, 0, sizeof(OSAudioBuffer));

    buf->bufferBytes = desc->bufferBytes;
    buf->format = desc->format;
    buf->dummy_data = (uint8_t*)malloc(desc->bufferBytes);
    memset(buf->dummy_data, 0, desc->bufferBytes);

    SDL_AudioFormat src_fmt = (desc->format.bitsPerSample == 16) ? AUDIO_S16SYS : AUDIO_U8;
    buf->stream = SDL_NewAudioStream(src_fmt, desc->format.channels, desc->format.sampleRate, MASTER_FORMAT, MASTER_CHANNELS, MASTER_FREQ);

    buf->device = device;
    buf->volume = SDL_MIX_MAXVOLUME;

    SDL_LockMutex(device->mutex);
    buf->next = device->buffers;
    if (device->buffers) device->buffers->prev = buf;
    device->buffers = buf;
    SDL_UnlockMutex(device->mutex);

    *buffer = buf;
    return OS_AUDIO_OK;
}

void os_audio_destroy_buffer(OSAudioBuffer* buffer) {
    if (buffer == NULL) return;

    OSAudioDevice* dev = buffer->device;
    if (dev) {
        SDL_LockMutex(dev->mutex);
        if (buffer->prev) {
            buffer->prev->next = buffer->next;
        } else if (dev->buffers == buffer) {
            dev->buffers = buffer->next;
        }
        if (buffer->next) {
            buffer->next->prev = buffer->prev;
        }
        SDL_UnlockMutex(dev->mutex);
    }

    SDL_FreeAudioStream(buffer->stream);
    free(buffer->dummy_data);
    free(buffer);
}

int os_audio_buffer_play(OSAudioBuffer* buffer, int flags) {
    if (buffer == NULL) return OS_AUDIO_ERR_UNKNOWN;
    buffer->looping = (flags & OS_AUDIO_PLAY_LOOPING) != 0;
    buffer->playing = true;
    return OS_AUDIO_OK;
}

int os_audio_buffer_stop(OSAudioBuffer* buffer) {
    if (buffer == NULL) return OS_AUDIO_ERR_UNKNOWN;
    buffer->playing = false;
    return OS_AUDIO_OK;
}

int os_audio_buffer_set_volume(OSAudioBuffer* buffer, int volume) {
    if (buffer == NULL) return OS_AUDIO_ERR_UNKNOWN;
    // volume is 0 to -10000
    if (volume == -10000) {
        buffer->volume = 0;
    } else {
        buffer->volume = (int)(pow(10.0, volume / 2000.0) * SDL_MIX_MAXVOLUME);
    }
    return OS_AUDIO_OK;
}

int os_audio_buffer_get_volume(OSAudioBuffer* buffer, int* volume) {
    if (buffer == NULL || volume == NULL) return OS_AUDIO_ERR_UNKNOWN;
    if (buffer->volume == 0) {
        *volume = -10000;
    } else {
        *volume = (int)(2000.0 * log10((double)buffer->volume / SDL_MIX_MAXVOLUME));
    }
    return OS_AUDIO_OK;
}

int os_audio_buffer_get_current_position(OSAudioBuffer* buffer, unsigned int* playPos, unsigned int* writePos) {
    if (buffer == NULL) return OS_AUDIO_ERR_UNKNOWN;
    if (playPos) *playPos = (unsigned int)buffer->playCursor % buffer->bufferBytes;
    if (writePos) *writePos = buffer->readCursor % buffer->bufferBytes;
    return OS_AUDIO_OK;
}

int os_audio_buffer_set_current_position(OSAudioBuffer* buffer, unsigned int playPos) {
    if (buffer == NULL) return OS_AUDIO_ERR_UNKNOWN;
    buffer->playCursor = playPos % buffer->bufferBytes;
    buffer->readCursor = playPos % buffer->bufferBytes;
    SDL_AudioStreamClear(buffer->stream);
    return OS_AUDIO_OK;
}

int os_audio_buffer_lock(OSAudioBuffer* buffer, unsigned int offset, unsigned int bytes, void** ptr1, unsigned int* bytes1, void** ptr2, unsigned int* bytes2) {
    if (buffer == NULL) return OS_AUDIO_ERR_UNKNOWN;
    
    offset %= buffer->bufferBytes;

    if (ptr1) *ptr1 = buffer->dummy_data + offset;
    
    if (offset + bytes <= buffer->bufferBytes) {
        if (bytes1) *bytes1 = bytes;
        if (ptr2) *ptr2 = NULL;
        if (bytes2) *bytes2 = 0;
    } else {
        if (bytes1) *bytes1 = buffer->bufferBytes - offset;
        if (ptr2) *ptr2 = buffer->dummy_data;
        if (bytes2) *bytes2 = bytes - *bytes1;
    }
    return OS_AUDIO_OK;
}

int os_audio_buffer_unlock(OSAudioBuffer* buffer, void* ptr1, unsigned int bytes1, void* ptr2, unsigned int bytes2) {
    // We already copied into dummy_data via the pointers returned by lock.
    return OS_AUDIO_OK;
}

int os_audio_buffer_get_status(OSAudioBuffer* buffer, bool* is_playing, bool* is_looping) {
    if (buffer == NULL) return OS_AUDIO_ERR_UNKNOWN;
    if (is_playing) *is_playing = buffer->playing;
    if (is_looping) *is_looping = buffer->looping;
    return OS_AUDIO_OK;
}
