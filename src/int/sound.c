#include "int/sound.h"

#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <SDL2/SDL.h>

#include "plib/gnw/debug.h"
#include "plib/gnw/memory.h"
#include "plib/gnw/winmain.h"
#include "plib/os/os_audio.h"
#include "plib/os/os_filesystem.h"

typedef struct FadeSound {
    Sound* sound;
    int deltaVolume;
    int targetVolume;
    int initialVolume;
    int currentVolume;
    int field_14;
    struct FadeSound* prev;
    struct FadeSound* next;
} FadeSound;

static_assert(sizeof(Sound) == 156, "wrong size");

static void* defaultMalloc(size_t size);
static void* defaultRealloc(void* ptr, size_t size);
static void defaultFree(void* ptr);
static long soundFileSize(int fileHandle);
static long soundTellData(int fileHandle);
static int soundWriteData(int fileHandle, const void* buf, unsigned int size);
static int soundReadData(int fileHandle, void* buf, unsigned int size);
static int soundOpenData(const char* filePath, int flags, ...);
static int soundSeekData(int fileHandle, long offset, int origin);
static int soundCloseData(int fileHandle);
static char* defaultMangler(char* fname);
static void refreshSoundBuffers(Sound* sound);
static int preloadBuffers(Sound* sound);
static int addSoundData(Sound* sound, unsigned char* buf, int size);
static Uint32 doTimerEvent(Uint32 interval, void* param);
static void removeTimedEvent(unsigned int* timerId);
static void removeFadeSound(FadeSound* fadeSound);
static void fadeSounds();
static int internalSoundFade(Sound* sound, int duration, int targetVolume, int a4);

// 0x51D478
static FadeSound* fadeHead = NULL;

// 0x51D47C
static FadeSound* fadeFreeList = NULL;

// 0x51D480
static unsigned int fadeEventHandle = UINT_MAX;

// 0x51D488
static MallocProc* mallocPtr = defaultMalloc;

// 0x51D48C
static ReallocProc* reallocPtr = defaultRealloc;

// 0x51D490
static FreeProc* freePtr = defaultFree;

// 0x51D494
static SoundFileIO defaultStream = {
    soundOpenData,
    soundCloseData,
    soundReadData,
    soundWriteData,
    soundSeekData,
    soundTellData,
    soundFileSize,
    -1,
};

// 0x51D4B4
static SoundFileNameMangler* nameMangler = defaultMangler;

// 0x51D4B8
static const char* errorMsgs[SOUND_ERR_COUNT] = {
    "sound.c: No error",
    "sound.c: SOS driver not loaded",
    "sound.c: SOS invalid pointer",
    "sound.c: SOS detect initialized",
    "sound.c: SOS fail on file open",
    "sound.c: SOS memory fail",
    "sound.c: SOS invalid driver ID",
    "sound.c: SOS no driver found",
    "sound.c: SOS detection failure",
    "sound.c: SOS driver loaded",
    "sound.c: SOS invalid handle",
    "sound.c: SOS no handles",
    "sound.c: SOS paused",
    "sound.c: SOS not paused",
    "sound.c: SOS invalid data",
    "sound.c: SOS drv file fail",
    "sound.c: SOS invalid port",
    "sound.c: SOS invalid IRQ",
    "sound.c: SOS invalid DMA",
    "sound.c: SOS invalid DMA IRQ",
    "sound.c: no device",
    "sound.c: not initialized",
    "sound.c: no sound",
    "sound.c: function not supported",
    "sound.c: no buffers available",
    "sound.c: file not found",
    "sound.c: already playing",
    "sound.c: not playing",
    "sound.c: already paused",
    "sound.c: not paused",
    "sound.c: invalid handle",
    "sound.c: no memory available",
    "sound.c: unknown error",
};

// 0x668150
static int soundErrorno;

// 0x668154
static int masterVol;

// 0x668158 (removed: primaryDSBuffer — no longer needed under SDL audio)

// 0x66815C
static int sampleRate;

// Number of sounds currently playing.
//
// 0x668160
static int numSounds;

// 0x668164
static int deviceInit;

// 0x668168
static int dataSize;

// 0x66816C
static int numBuffers;

// 0x668170
static bool driverInit;

// 0x668174
static Sound* soundMgrList;

// 0x668178
OSAudioDevice* soundDSObject;

// 0x4AC6F0
static void* defaultMalloc(size_t size)
{
    return malloc(size);
}

// 0x4AC6F8
static void* defaultRealloc(void* ptr, size_t size)
{
    return realloc(ptr, size);
}

// 0x4AC700
static void defaultFree(void* ptr)
{
    free(ptr);
}

// 0x4AC708
void soundRegisterAlloc(MallocProc* mallocProc, ReallocProc* reallocProc, FreeProc* freeProc)
{
    mallocPtr = mallocProc;
    reallocPtr = reallocProc;
    freePtr = freeProc;
}

// 0x4AC71C
static long soundFileSize(int fileHandle)
{
    long pos;
    long size;

    pos = os_filesystem_tell(fileHandle);
    size = lseek(fileHandle, 0, SEEK_END);
    lseek(fileHandle, pos, SEEK_SET);

    return size;
}

// 0x4AC750
static long soundTellData(int fileHandle)
{
    return os_filesystem_tell(fileHandle);
}

// 0x4AC758
static int soundWriteData(int fileHandle, const void* buf, unsigned int size)
{
    return write(fileHandle, buf, size);
}

// 0x4AC760
static int soundReadData(int fileHandle, void* buf, unsigned int size)
{
    return read(fileHandle, buf, size);
}

// 0x4AC768
static int soundOpenData(const char* filePath, int flags, ...)
{
    return open(filePath, flags);
}

// 0x4AC774
static int soundSeekData(int fileHandle, long offset, int origin)
{
    return lseek(fileHandle, offset, origin);
}

// 0x4AC77C
static int soundCloseData(int fileHandle)
{
    return close(fileHandle);
}

// 0x4AC78C
static char* defaultMangler(char* fname)
{
    return fname;
}

// 0x4AC790
const char* soundError(int err)
{
    if (err == -1) {
        err = soundErrorno;
    }

    if (err < 0 || err > SOUND_UNKNOWN_ERROR) {
        err = SOUND_UNKNOWN_ERROR;
    }

    return errorMsgs[err];
}

// 0x4AC7B0
static void refreshSoundBuffers(Sound* sound)
{
    if (sound->field_3C & 0x80) {
        return;
    }

    unsigned int readPos;
    unsigned int writePos;
    int hr = os_audio_buffer_get_current_position(sound->directSoundBuffer, &readPos, &writePos);
    if (hr != OS_AUDIO_OK) {
        return;
    }

    if (readPos < sound->field_74) {
        sound->field_64 += readPos + sound->field_78 * sound->field_7C - sound->field_74;
    } else {
        sound->field_64 += readPos - sound->field_74;
    }

    if (sound->field_3C & 0x0100) {
        if (sound->field_44 & 0x20) {
            if (sound->field_3C & 0x0200) {
                sound->field_3C |= 0x80;
            }
        } else {
            if (sound->field_60 <= sound->field_64) {
                sound->field_3C |= 0x0280;
            }
        }
    }
    sound->field_74 = readPos;

    if (sound->field_60 < sound->field_64) {
        int v3;
        do {
            v3 = sound->field_64 - sound->field_60;
            sound->field_64 = v3;
        } while (v3 > sound->field_60);
    }

    int v6 = readPos / sound->field_7C;
    if (sound->field_70 == v6) {
        return;
    }

    int v53;
    if (sound->field_70 > v6) {
        v53 = v6 + sound->field_78 - sound->field_70;
    } else {
        v53 = v6 - sound->field_70;
    }

    if (sound->field_7C * v53 >= sound->readLimit) {
        v53 = (sound->readLimit + sound->field_7C - 1) / sound->field_7C;
    }

    if (v53 < sound->field_5C) {
        return;
    }

    void* audioPtr1;
    void* audioPtr2;
    unsigned int audioBytes1;
    unsigned int audioBytes2;
    hr = os_audio_buffer_lock(sound->directSoundBuffer, sound->field_7C * sound->field_70, sound->field_7C * v53, &audioPtr1, &audioBytes1, &audioPtr2, &audioBytes2);
    if (hr == OS_AUDIO_ERR_BUFFERLOST) {
        hr = os_audio_buffer_lock(sound->directSoundBuffer, sound->field_7C * sound->field_70, sound->field_7C * v53, &audioPtr1, &audioBytes1, &audioPtr2, &audioBytes2);
    }

    if (hr != OS_AUDIO_OK) {
        return;
    }

    if (audioBytes1 + audioBytes2 != sound->field_7C * v53) {
        debug_printf("locked memory region not big enough, wanted %d (%d * %d), got %d (%d + %d)\n", sound->field_7C * v53, v53, sound->field_7C, audioBytes1 + audioBytes2, audioBytes1, audioBytes2);
        debug_printf("Resetting readBuffers from %d to %d\n", v53, (audioBytes1 + audioBytes2) / sound->field_7C);

        v53 = (audioBytes1 + audioBytes2) / sound->field_7C;
        if (v53 < sound->field_5C) {
            debug_printf("No longer above read buffer size, returning\n");
            return;
        }
    }
    unsigned char* audioPtr = (unsigned char*)audioPtr1;
    int audioBytes = audioBytes1;
    while (--v53 != -1) {
        int bytesRead;
        if (sound->field_3C & 0x0200) {
            bytesRead = sound->field_7C;
            memset(sound->field_20, 0, bytesRead);
        } else {
            int bytesToRead = sound->field_7C;
            if (sound->field_58 != -1) {
                int pos = sound->io.tell(sound->io.fd);
                if (bytesToRead + pos > sound->field_58) {
                    bytesToRead = sound->field_58 - pos;
                }
            }

            bytesRead = sound->io.read(sound->io.fd, sound->field_20, bytesToRead);
            if (bytesRead < sound->field_7C) {
                if (!(sound->field_3C & 0x20) || (sound->field_3C & 0x0100)) {
                    memset(sound->field_20 + bytesRead, 0, sound->field_7C - bytesRead);
                    sound->field_3C |= 0x0200;
                    bytesRead = sound->field_7C;
                } else {
                    while (bytesRead < sound->field_7C) {
                        if (sound->field_50 == -1) {
                            sound->io.seek(sound->io.fd, sound->field_54, SEEK_SET);
                            if (sound->callback != NULL) {
                                sound->callback(sound->callbackUserData, 0x0400);
                            }
                        } else {
                            if (sound->field_50 <= 0) {
                                sound->field_58 = -1;
                                sound->field_54 = 0;
                                sound->field_50 = 0;
                                sound->field_3C &= ~0x20;
                                bytesRead += sound->io.read(sound->io.fd, sound->field_20 + bytesRead, sound->field_7C - bytesRead);
                                break;
                            }

                            sound->field_50--;
                            sound->io.seek(sound->io.fd, sound->field_54, SEEK_SET);

                            if (sound->callback != NULL) {
                                sound->callback(sound->callbackUserData, 0x400);
                            }
                        }

                        if (sound->field_58 == -1) {
                            bytesToRead = sound->field_7C - bytesRead;
                        } else {
                            int pos = sound->io.tell(sound->io.fd);
                            if (sound->field_7C + bytesRead + pos <= sound->field_58) {
                                bytesToRead = sound->field_7C - bytesRead;
                            } else {
                                bytesToRead = sound->field_58 - bytesRead - pos;
                            }
                        }

                        int v20 = sound->io.read(sound->io.fd, sound->field_20 + bytesRead, bytesToRead);
                        bytesRead += v20;
                        if (v20 < bytesToRead) {
                            break;
                        }
                    }
                }
            }
        }

        if (bytesRead > audioBytes) {
            if (audioBytes != 0) {
                memcpy(audioPtr, sound->field_20, audioBytes);
            }

            if (audioPtr2 != NULL) {
                memcpy(audioPtr2, sound->field_20 + audioBytes, bytesRead - audioBytes);
                audioPtr = (unsigned char*)audioPtr2 + bytesRead - audioBytes;
                audioBytes = audioBytes2 - bytesRead;
            } else {
                debug_printf("Hm, no second write pointer, but buffer not big enough, this shouldn't happen\n");
            }
        } else {
            memcpy(audioPtr, sound->field_20, bytesRead);
            audioPtr += bytesRead;
            audioBytes -= bytesRead;
        }
    }

    os_audio_buffer_unlock(sound->directSoundBuffer, audioPtr1, audioBytes1, audioPtr2, audioBytes2);

    sound->field_70 = v6;

    return;
}

// 0x4ACC58
int soundInit(int a1, int a2, int a3, int a4, int rate)
{
    if (os_audio_create_device(&soundDSObject) != OS_AUDIO_OK) {
        soundDSObject = NULL;
        soundErrorno = SOUND_SOS_DETECTION_FAILURE;
        return soundErrorno;
    }

    sampleRate = rate;
    dataSize = a4;
    numBuffers = a2;
    driverInit = true;
    deviceInit = 1;

    soundSetMasterVolume(VOLUME_MAX);
    soundErrorno = SOUND_NO_ERROR;

    return 0;
}

// 0x4AD04C
void soundClose()
{
    while (soundMgrList != NULL) {
        Sound* next = soundMgrList->next;
        soundDelete(soundMgrList);
        soundMgrList = next;
    }

    if (fadeEventHandle != -1) {
        removeTimedEvent(&fadeEventHandle);
    }

    while (fadeFreeList != NULL) {
        FadeSound* next = fadeFreeList->next;
        freePtr(fadeFreeList);
        fadeFreeList = next;
    }

    if (soundDSObject != NULL) {
        os_audio_destroy_device(soundDSObject);
        soundDSObject = NULL;
    }

    soundErrorno = SOUND_NO_ERROR;
    driverInit = false;
}

// 0x4AD0FC
Sound* soundAllocate(int a1, int a2)
{
    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return NULL;
    }

    Sound* sound = (Sound*)mallocPtr(sizeof(*sound));
    memset(sound, 0, sizeof(*sound));

    memcpy(&(sound->io), &defaultStream, sizeof(defaultStream));

    OSAudioWaveFormat wfxFormat;
    memset(&wfxFormat, 0, sizeof(wfxFormat));
    wfxFormat.channels = 1;

    if (a2 & 0x08) {
        wfxFormat.bitsPerSample = 16;
    } else {
        wfxFormat.bitsPerSample = 8;
    }

    if (!(a2 & 0x02)) {
        a2 |= 0x02;
    }

    wfxFormat.sampleRate = sampleRate;

    sound->field_3C = a2;
    sound->field_44 = a1;
    sound->field_7C = dataSize;
    sound->field_64 = 0;
    sound->directSoundBuffer = NULL;
    sound->field_40 = 0;
    sound->directSoundBufferDescription.format = wfxFormat;
    sound->field_78 = numBuffers;
    sound->readLimit = sound->field_7C * numBuffers;

    if (a1 & 0x10) {
        sound->field_50 = -1;
        sound->field_3C |= 0x20;
    }

    sound->field_58 = -1;
    sound->field_5C = 1;
    sound->volume = VOLUME_MAX;
    sound->prev = NULL;
    sound->field_54 = 0;
    sound->next = soundMgrList;

    if (soundMgrList != NULL) {
        soundMgrList->prev = sound;
    }

    soundMgrList = sound;

    return sound;
}

// 0x4AD308
static int preloadBuffers(Sound* sound)
{
    unsigned char* buf;
    int bytes_read;
    int result;
    int v15;
    unsigned char* v14;
    int size;

    size = sound->io.filelength(sound->io.fd);
    sound->field_60 = size;

    if (sound->field_44 & 0x02) {
        if (!(sound->field_3C & 0x20)) {
            sound->field_3C |= 0x0120;
        }

        if (sound->field_78 * sound->field_7C >= size) {
            if (size / sound->field_7C * sound->field_7C != size) {
                size = (size / sound->field_7C + 1) * sound->field_7C;
            }
        } else {
            size = sound->field_78 * sound->field_7C;
        }
    } else {
        sound->field_44 &= ~(0x03);
        sound->field_44 |= 0x01;
    }

    buf = (unsigned char*)mallocPtr(size);
    bytes_read = sound->io.read(sound->io.fd, buf, size);
    if (bytes_read != size) {
        if (!(sound->field_3C & 0x20) || (sound->field_3C & (0x01 << 8))) {
            memset(buf + bytes_read, 0, size - bytes_read);
        } else {
            v14 = buf + bytes_read;
            v15 = bytes_read;
            while (size - v15 > bytes_read) {
                memcpy(v14, buf, bytes_read);
                v15 += bytes_read;
                v14 += bytes_read;
            }

            if (v15 < size) {
                memcpy(v14, buf, size - v15);
            }
        }
    }

    result = soundSetData(sound, buf, size);
    freePtr(buf);

    if (sound->field_44 & 0x01) {
        sound->io.close(sound->io.fd);
        sound->io.fd = -1;
    } else {
        if (sound->field_20 == NULL) {
            sound->field_20 = (unsigned char*)mallocPtr(sound->field_7C);
        }
    }

    return result;
}

// 0x4AD498
int soundLoad(Sound* sound, char* filePath)
{
    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    sound->io.fd = sound->io.open(nameMangler(filePath), 0x0200);
    if (sound->io.fd == -1) {
        soundErrorno = SOUND_FILE_NOT_FOUND;
        return soundErrorno;
    }

    return preloadBuffers(sound);
}

// 0x4AD504
int soundRewind(Sound* sound)
{
    int hr;

    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL || sound->directSoundBuffer == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    if (sound->field_44 & 0x02) {
        sound->io.seek(sound->io.fd, 0, SEEK_SET);
        sound->field_70 = 0;
        sound->field_74 = 0;
        sound->field_64 = 0;
        sound->field_3C &= 0xFD7F;
        hr = os_audio_buffer_set_current_position(sound->directSoundBuffer, 0);
        preloadBuffers(sound);
    } else {
        hr = os_audio_buffer_set_current_position(sound->directSoundBuffer, 0);
    }

    if (hr != OS_AUDIO_OK) {
        soundErrorno = SOUND_UNKNOWN_ERROR;
        return soundErrorno;
    }

    sound->field_40 &= ~SOUND_FLAG_SOUND_IS_DONE;

    soundErrorno = SOUND_NO_ERROR;
    return soundErrorno;
}

// 0x4AD5C8
static int addSoundData(Sound* sound, unsigned char* buf, int size)
{
    int hr;
    void* audio_ptr_1;
    unsigned int audio_bytes_1;
    void* audio_ptr_2;
    unsigned int audio_bytes_2;

    hr = os_audio_buffer_lock(sound->directSoundBuffer, 0, size, &audio_ptr_1, &audio_bytes_1, &audio_ptr_2, &audio_bytes_2);
    if (hr == OS_AUDIO_ERR_BUFFERLOST) {
        hr = os_audio_buffer_lock(sound->directSoundBuffer, 0, size, &audio_ptr_1, &audio_bytes_1, &audio_ptr_2, &audio_bytes_2);
    }

    if (hr != OS_AUDIO_OK) {
        soundErrorno = SOUND_UNKNOWN_ERROR;
        return soundErrorno;
    }

    memcpy(audio_ptr_1, buf, audio_bytes_1);

    if (audio_ptr_2 != NULL) {
        memcpy(audio_ptr_2, buf + audio_bytes_1, audio_bytes_2);
    }

    hr = os_audio_buffer_unlock(sound->directSoundBuffer, audio_ptr_1, audio_bytes_1, audio_ptr_2, audio_bytes_2);
    if (hr != OS_AUDIO_OK) {
        soundErrorno = SOUND_UNKNOWN_ERROR;
        return soundErrorno;
    }

    soundErrorno = SOUND_NO_ERROR;
    return soundErrorno;
}

// 0x4AD6C0
int soundSetData(Sound* sound, unsigned char* buf, int size)
{
    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    if (sound->directSoundBuffer == NULL) {
        sound->directSoundBufferDescription.bufferBytes = size;

        if (os_audio_create_buffer(soundDSObject, &(sound->directSoundBufferDescription), &(sound->directSoundBuffer)) != OS_AUDIO_OK) {
            soundErrorno = SOUND_UNKNOWN_ERROR;
            return soundErrorno;
        }
    }

    return addSoundData(sound, buf, size);
}

// 0x4AD73C
int soundPlay(Sound* sound)
{
    int hr;
    unsigned int readPos;
    unsigned int writePos;

    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL || sound->directSoundBuffer == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    // TODO: Check.
    if (sound->field_40 & SOUND_FLAG_SOUND_IS_DONE) {
        soundRewind(sound);
    }

    soundVolume(sound, sound->volume);

    hr = os_audio_buffer_play(sound->directSoundBuffer, sound->field_3C & 0x20 ? OS_AUDIO_PLAY_LOOPING : 0);

    os_audio_buffer_get_current_position(sound->directSoundBuffer, &readPos, &writePos);
    sound->field_70 = readPos / sound->field_7C;

    if (hr != OS_AUDIO_OK) {
        soundErrorno = SOUND_UNKNOWN_ERROR;
        return soundErrorno;
    }

    sound->field_40 |= SOUND_FLAG_SOUND_IS_PLAYING;

    ++numSounds;

    soundErrorno = SOUND_NO_ERROR;
    return soundErrorno;
}

// 0x4AD828
int soundStop(Sound* sound)
{
    int hr;

    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL || sound->directSoundBuffer == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    if (!(sound->field_40 & SOUND_FLAG_SOUND_IS_PLAYING)) {
        soundErrorno = SOUND_NOT_PLAYING;
        return soundErrorno;
    }

    hr = os_audio_buffer_stop(sound->directSoundBuffer);
    if (hr != OS_AUDIO_OK) {
        soundErrorno = SOUND_UNKNOWN_ERROR;
        return soundErrorno;
    }

    sound->field_40 &= ~SOUND_FLAG_SOUND_IS_PLAYING;
    numSounds--;

    soundErrorno = SOUND_NO_ERROR;
    return soundErrorno;
}

// 0x4AD8DC
int soundDelete(Sound* sample)
{
    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sample == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    if (sample->io.fd != -1) {
        sample->io.close(sample->io.fd);
        sample->io.fd = -1;
    }

    soundMgrDelete(sample);

    soundErrorno = SOUND_NO_ERROR;
    return soundErrorno;
}

// 0x4AD948
int soundContinue(Sound* sound)
{
    int hr;
    bool is_playing;
    bool is_looping;

    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    if (sound->directSoundBuffer == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    if (!(sound->field_40 & SOUND_FLAG_SOUND_IS_PLAYING) || (sound->field_40 & SOUND_FLAG_SOUND_IS_PAUSED)) {
        soundErrorno = SOUND_NOT_PLAYING;
        return soundErrorno;
    }

    if (sound->field_40 & SOUND_FLAG_SOUND_IS_DONE) {
        soundErrorno = SOUND_UNKNOWN_ERROR;
        return soundErrorno;
    }

    hr = os_audio_buffer_get_status(sound->directSoundBuffer, &is_playing, &is_looping);
    if (hr != OS_AUDIO_OK) {
        debug_printf("Error in soundContinue, %x\n", hr);

        soundErrorno = SOUND_UNKNOWN_ERROR;
        return soundErrorno;
    }

    if (!(sound->field_3C & 0x80) && (is_playing || is_looping)) {
        if (!(sound->field_40 & SOUND_FLAG_SOUND_IS_PAUSED) && (sound->field_44 & 0x02)) {
            refreshSoundBuffers(sound);
        }
    } else if (!(sound->field_40 & SOUND_FLAG_SOUND_IS_PAUSED)) {
        if (sound->callback != NULL) {
            sound->callback(sound->callbackUserData, 1);
            sound->callback = NULL;
        }

        if (sound->field_44 & 0x04) {
            sound->callback = NULL;
            soundDelete(sound);
        } else {
            sound->field_40 |= SOUND_FLAG_SOUND_IS_DONE;

            if (sound->field_40 & SOUND_FLAG_SOUND_IS_PLAYING) {
                --numSounds;
            }

            soundStop(sound);

            sound->field_40 &= ~(SOUND_FLAG_SOUND_IS_DONE | SOUND_FLAG_SOUND_IS_PLAYING);
        }
    }

    soundErrorno = SOUND_NO_ERROR;
    return soundErrorno;
}

// 0x4ADA84
bool soundPlaying(Sound* sound)
{
    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return false;
    }

    if (sound == NULL || sound->directSoundBuffer == 0) {
        soundErrorno = SOUND_NO_SOUND;
        return false;
    }

    return (sound->field_40 & SOUND_FLAG_SOUND_IS_PLAYING) != 0;
}

// 0x4ADAC4
bool soundDone(Sound* sound)
{
    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return false;
    }

    if (sound == NULL || sound->directSoundBuffer == 0) {
        soundErrorno = SOUND_NO_SOUND;
        return false;
    }

    return sound->field_40 & 1;
}

// 0x4ADB44
bool soundPaused(Sound* sound)
{
    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return false;
    }

    if (sound == NULL || sound->directSoundBuffer == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return false;
    }

    return (sound->field_40 & SOUND_FLAG_SOUND_IS_PAUSED) != 0;
}

// 0x4ADBC4
int soundType(Sound* sound, int a2)
{
    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return 0;
    }

    if (sound == NULL || sound->directSoundBuffer == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return 0;
    }

    return sound->field_44 & a2;
}

// 0x4ADC04
int soundLength(Sound* sound)
{
    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL || sound->directSoundBuffer == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    int bytesPerSec = sound->directSoundBufferDescription.format.channels * (sound->directSoundBufferDescription.format.bitsPerSample / 8) * sound->directSoundBufferDescription.format.sampleRate;
    int v3 = sound->field_60;
    int v4 = v3 % bytesPerSec;
    int result = v3 / bytesPerSec;
    if (v4 != 0) {
        result += 1;
    }

    return result;
}

// 0x4ADD00
int soundLoop(Sound* sound, int a2)
{
    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    if (a2) {
        sound->field_3C |= 0x20;
        sound->field_50 = a2;
    } else {
        sound->field_50 = 0;
        sound->field_58 = -1;
        sound->field_54 = 0;
        sound->field_3C &= ~(0x20);
    }

    soundErrorno = SOUND_NO_ERROR;
    return soundErrorno;
}

// Normalize volume?
//
// 0x4ADD68
int soundVolumeHMItoDirectSound(int volume)
{
    double normalizedVolume;

    if (volume > VOLUME_MAX) {
        volume = VOLUME_MAX;
    }

    if (volume != 0) {
        normalizedVolume = -1000.0 * log2(32767.0 / volume);
        normalizedVolume = max(min(normalizedVolume, 0.0), -10000.0);
    } else {
        normalizedVolume = -10000.0;
    }

    return (int)normalizedVolume;
}

// 0x4ADE0C
int soundVolume(Sound* sound, int volume)
{
    int normalizedVolume;
    int hr;

    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    sound->volume = volume;

    if (sound->directSoundBuffer == NULL) {
        soundErrorno = SOUND_NO_ERROR;
        return soundErrorno;
    }

    normalizedVolume = soundVolumeHMItoDirectSound(masterVol * volume / VOLUME_MAX);

    hr = os_audio_buffer_set_volume(sound->directSoundBuffer, normalizedVolume);
    if (hr != OS_AUDIO_OK) {
        soundErrorno = SOUND_UNKNOWN_ERROR;
        return soundErrorno;
    }

    soundErrorno = SOUND_NO_ERROR;
    return soundErrorno;
}

// 0x4ADE80
int soundGetVolume(Sound* sound)
{
    long volume;
    int v13;
    int v8;
    int diff;

    if (!deviceInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL || sound->directSoundBuffer == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    {
        int tmpVol = 0;
        os_audio_buffer_get_volume(sound->directSoundBuffer, &tmpVol);
        volume = tmpVol;
    }

    if (volume == -10000) {
        v13 = 0;
    } else {
        // TODO: Check.
        volume = -volume;
        v13 = (int)(32767.0 / pow(2.0, (volume * 0.001)));
    }

    v8 = VOLUME_MAX * v13 / masterVol;
    diff = abs(v8 - sound->volume);
    if (diff > 20) {
        debug_printf("Actual sound volume differs significantly from noted volume actual %x stored %x, diff %d.", v8, sound->volume, diff);
    }

    return sound->volume;
}

// 0x4ADFF0
int soundSetCallback(Sound* sound, SoundCallback* callback, void* userData)
{
    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    sound->callback = callback;
    sound->callbackUserData = userData;

    soundErrorno = SOUND_NO_ERROR;
    return soundErrorno;
}

// 0x4AE02C
int soundSetChannel(Sound* sound, int channels)
{
    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    if (channels == 3) {
        sound->directSoundBufferDescription.format.channels = 2;
    }

    soundErrorno = SOUND_NO_ERROR;
    return soundErrorno;
}

// 0x4AE0B0
int soundSetReadLimit(Sound* sound, int readLimit)
{
    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL) {
        soundErrorno = SOUND_NO_DEVICE;
        return soundErrorno;
    }

    sound->readLimit = readLimit;

    soundErrorno = SOUND_NO_ERROR;
    return soundErrorno;
}

// TODO: Check, looks like it uses couple of inlined functions.
//
// 0x4AE0E4
int soundPause(Sound* sound)
{
    int hr;
    unsigned int readPos;
    unsigned int writePos;

    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    if (sound->directSoundBuffer == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    if (!(sound->field_40 & SOUND_FLAG_SOUND_IS_PLAYING)) {
        soundErrorno = SOUND_NOT_PLAYING;
        return soundErrorno;
    }

    if (sound->field_40 & SOUND_FLAG_SOUND_IS_PAUSED) {
        soundErrorno = SOUND_ALREADY_PAUSED;
        return soundErrorno;
    }

    hr = os_audio_buffer_get_current_position(sound->directSoundBuffer, &readPos, &writePos);
    if (hr != OS_AUDIO_OK) {
        soundErrorno = SOUND_UNKNOWN_ERROR;
        return soundErrorno;
    }

    sound->field_48 = readPos;
    sound->field_40 |= SOUND_FLAG_SOUND_IS_PAUSED;

    return soundStop(sound);
}

// TODO: Check, looks like it uses couple of inlined functions.
//
// 0x4AE1F0
int soundUnpause(Sound* sound)
{
    int hr;

    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL || sound->directSoundBuffer == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    if ((sound->field_40 & SOUND_FLAG_SOUND_IS_PLAYING) != 0) {
        soundErrorno = SOUND_NOT_PAUSED;
        return soundErrorno;
    }

    if (!(sound->field_40 & SOUND_FLAG_SOUND_IS_PAUSED)) {
        soundErrorno = SOUND_NOT_PAUSED;
        return soundErrorno;
    }

    hr = os_audio_buffer_set_current_position(sound->directSoundBuffer, sound->field_48);
    if (hr != OS_AUDIO_OK) {
        soundErrorno = SOUND_UNKNOWN_ERROR;
        return soundErrorno;
    }

    sound->field_40 &= ~SOUND_FLAG_SOUND_IS_PAUSED;
    sound->field_48 = 0;

    return soundPlay(sound);
}

// 0x4AE2FC
int soundSetFileIO(Sound* sound, SoundOpenProc* openProc, SoundCloseProc* closeProc, SoundReadProc* readProc, SoundWriteProc* writeProc, SoundSeekProc* seekProc, SoundTellProc* tellProc, SoundFileLengthProc* fileLengthProc)
{
    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    if (openProc != NULL) {
        sound->io.open = openProc;
    }

    if (closeProc != NULL) {
        sound->io.close = closeProc;
    }

    if (readProc != NULL) {
        sound->io.read = readProc;
    }

    if (writeProc != NULL) {
        sound->io.write = writeProc;
    }

    if (seekProc != NULL) {
        sound->io.seek = seekProc;
    }

    if (tellProc != NULL) {
        sound->io.tell = tellProc;
    }

    if (fileLengthProc != NULL) {
        sound->io.filelength = fileLengthProc;
    }

    soundErrorno = SOUND_NO_ERROR;
    return soundErrorno;
}

// 0x4AE378
void soundMgrDelete(Sound* sound)
{
    FadeSound* curr;
    Sound* v10;
    Sound* v11;

    if (sound->field_40 & SOUND_FLAG_SOUND_IS_FADING) {
        curr = fadeHead;

        while (curr != NULL) {
            if (sound == curr->sound) {
                break;
            }

            curr = curr->next;
        }

        removeFadeSound(curr);
    }

    if (sound->directSoundBuffer != NULL) {
        // NOTE: Uninline.
        if (!soundPlaying(sound)) {
            soundStop(sound);
        }

        if (sound->callback != NULL) {
            sound->callback(sound->callbackUserData, 1);
        }

        os_audio_destroy_buffer(sound->directSoundBuffer);
        sound->directSoundBuffer = NULL;
    }

    if (sound->field_90 != NULL) {
        sound->field_90(sound->field_8C);
    }

    if (sound->field_20 != NULL) {
        freePtr(sound->field_20);
        sound->field_20 = NULL;
    }

    v10 = sound->next;
    if (v10 != NULL) {
        v10->prev = sound->prev;
    }

    v11 = sound->prev;
    if (v11 != NULL) {
        v11->next = sound->next;
    } else {
        soundMgrList = sound->next;
    }

    freePtr(sound);
}

// 0x4AE578
int soundSetMasterVolume(int volume)
{
    if (volume < VOLUME_MIN || volume > VOLUME_MAX) {
        soundErrorno = SOUND_UNKNOWN_ERROR;
        return soundErrorno;
    }

    masterVol = volume;

    Sound* curr = soundMgrList;
    while (curr != NULL) {
        soundVolume(curr, curr->volume);
        curr = curr->next;
    }

    soundErrorno = SOUND_NO_ERROR;
    return soundErrorno;
}

// 0x4AE5C8
static Uint32 doTimerEvent(Uint32 interval, void* param)
{
    if (param != NULL) {
        void (*fn)() = (void (*)())param;
        fn();
    }
    return interval;
}

// 0x4AE614
static void removeTimedEvent(unsigned int* timerId)
{
    if (*timerId != (unsigned int)-1) {
        SDL_RemoveTimer((SDL_TimerID)*timerId);
        *timerId = -1;
    }
}

// 0x4AE634
int soundGetPosition(Sound* sound)
{
    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    unsigned int playPos;
    unsigned int writePos;
    os_audio_buffer_get_current_position(sound->directSoundBuffer, &playPos, &writePos);

    if ((sound->field_44 & 0x02) != 0) {
        if (playPos < sound->field_74) {
            playPos += sound->field_64 + sound->field_78 * sound->field_7C - sound->field_74;
        } else {
            playPos -= sound->field_74 + sound->field_64;
        }
    }

    return playPos;
}

// 0x4AE6CC
int soundSetPosition(Sound* sound, int a2)
{
    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    if (sound->directSoundBuffer == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    if (sound->field_44 & 0x02) {
        int v6 = a2 / sound->field_7C % sound->field_78;

        os_audio_buffer_set_current_position(sound->directSoundBuffer, v6 * sound->field_7C + a2 % sound->field_7C);

        sound->io.seek(sound->io.fd, v6 * sound->field_7C, SEEK_SET);
        int bytes_read = sound->io.read(sound->io.fd, sound->field_20, sound->field_7C);
        if (bytes_read < sound->field_7C) {
            if (sound->field_44 & 0x02) {
                sound->io.seek(sound->io.fd, 0, SEEK_SET);
                sound->io.read(sound->io.fd, sound->field_20 + bytes_read, sound->field_7C - bytes_read);
            } else {
                memset(sound->field_20 + bytes_read, 0, sound->field_7C - bytes_read);
            }
        }

        int v17 = v6 + 1;
        sound->field_64 = a2;

        if (v17 < sound->field_78) {
            sound->field_70 = v17;
        } else {
            sound->field_70 = 0;
        }

        soundContinue(sound);
    } else {
        os_audio_buffer_set_current_position(sound->directSoundBuffer, a2);
    }

    soundErrorno = SOUND_NO_ERROR;
    return soundErrorno;
}

// 0x4AE830
static void removeFadeSound(FadeSound* fadeSound)
{
    FadeSound* prev;
    FadeSound* next;
    FadeSound* tmp;

    if (fadeSound == NULL) {
        return;
    }

    if (fadeSound->sound == NULL) {
        return;
    }

    if (!(fadeSound->sound->field_40 & SOUND_FLAG_SOUND_IS_FADING)) {
        return;
    }

    prev = fadeSound->prev;
    if (prev != NULL) {
        prev->next = fadeSound->next;
    } else {
        fadeHead = fadeSound->next;
    }

    next = fadeSound->next;
    if (next != NULL) {
        next->prev = fadeSound->prev;
    }

    fadeSound->sound->field_40 &= ~SOUND_FLAG_SOUND_IS_FADING;
    fadeSound->sound = NULL;

    tmp = fadeFreeList;
    fadeFreeList = fadeSound;
    fadeSound->next = tmp;
}

// 0x4AE8B0
static void fadeSounds()
{
    FadeSound* ptr;

    ptr = fadeHead;
    while (ptr != NULL) {
        if ((ptr->currentVolume > ptr->targetVolume || ptr->currentVolume + ptr->deltaVolume < ptr->targetVolume) && (ptr->currentVolume < ptr->targetVolume || ptr->currentVolume + ptr->deltaVolume > ptr->targetVolume)) {
            ptr->currentVolume += ptr->deltaVolume;
            soundVolume(ptr->sound, ptr->currentVolume);
        } else {
            if (ptr->targetVolume == 0) {
                if (ptr->field_14) {
                    soundPause(ptr->sound);
                    soundVolume(ptr->sound, ptr->initialVolume);
                } else {
                    if (ptr->sound->field_44 & 0x04) {
                        soundDelete(ptr->sound);
                    } else {
                        soundStop(ptr->sound);

                        ptr->initialVolume = ptr->targetVolume;
                        ptr->currentVolume = ptr->targetVolume;
                        ptr->deltaVolume = 0;

                        soundVolume(ptr->sound, ptr->targetVolume);
                    }
                }
            }

            removeFadeSound(ptr);
        }
    }

    if (fadeHead == NULL) {
        // NOTE: Uninline.
        removeTimedEvent(&fadeEventHandle);
    }
}

// 0x4AE988
static int internalSoundFade(Sound* sound, int duration, int targetVolume, int a4)
{
    FadeSound* ptr;

    if (!deviceInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    ptr = NULL;
    if (sound->field_40 & SOUND_FLAG_SOUND_IS_FADING) {
        ptr = fadeHead;
        while (ptr != NULL) {
            if (ptr->sound == sound) {
                break;
            }

            ptr = ptr->next;
        }
    }

    if (ptr == NULL) {
        if (fadeFreeList != NULL) {
            ptr = fadeFreeList;
            fadeFreeList = fadeFreeList->next;
        } else {
            ptr = (FadeSound*)mallocPtr(sizeof(FadeSound));
        }

        if (ptr != NULL) {
            if (fadeHead != NULL) {
                fadeHead->prev = ptr;
            }

            ptr->sound = sound;
            ptr->prev = NULL;
            ptr->next = fadeHead;
            fadeHead = ptr;
        }
    }

    if (ptr == NULL) {
        soundErrorno = SOUND_NO_MEMORY_AVAILABLE;
        return soundErrorno;
    }

    ptr->targetVolume = targetVolume;
    ptr->initialVolume = soundGetVolume(sound);
    ptr->currentVolume = ptr->initialVolume;
    ptr->field_14 = a4;
    // TODO: Check.
    ptr->deltaVolume = 8 * (125 * (targetVolume - ptr->initialVolume)) / (40 * duration);

    sound->field_40 |= SOUND_FLAG_SOUND_IS_FADING;

    bool v14;
    if (driverInit) {
        if (sound->directSoundBuffer != NULL) {
            v14 = (sound->field_40 & SOUND_FLAG_SOUND_IS_PLAYING) == 0;
        } else {
            soundErrorno = SOUND_NO_SOUND;
            v14 = true;
        }
    } else {
        soundErrorno = SOUND_NOT_INITIALIZED;
        v14 = true;
    }

    if (v14) {
        soundPlay(sound);
    }

    if (fadeEventHandle != -1) {
        soundErrorno = SOUND_NO_ERROR;
        return soundErrorno;
    }

    fadeEventHandle = (unsigned int)SDL_AddTimer(40, doTimerEvent, (void*)fadeSounds);
    if (fadeEventHandle == 0) {
        soundErrorno = SOUND_UNKNOWN_ERROR;
        return soundErrorno;
    }

    soundErrorno = SOUND_NO_ERROR;
    return soundErrorno;
}

// 0x4AEB0C
int soundFade(Sound* sound, int duration, int targetVolume)
{
    return internalSoundFade(sound, duration, targetVolume, 0);
}

// 0x4AEB54
void soundFlushAllSounds()
{
    while (soundMgrList != NULL) {
        soundDelete(soundMgrList);
    }
}

// 0x4AEBE0
void soundContinueAll()
{
    Sound* curr = soundMgrList;
    while (curr != NULL) {
        // Sound can be deallocated in `soundContinue`.
        Sound* next = curr->next;
        soundContinue(curr);
        curr = next;
    }
}

// 0x4AEC00
int soundSetDefaultFileIO(SoundOpenProc* openProc, SoundCloseProc* closeProc, SoundReadProc* readProc, SoundWriteProc* writeProc, SoundSeekProc* seekProc, SoundTellProc* tellProc, SoundFileLengthProc* fileLengthProc)
{
    if (openProc != NULL) {
        defaultStream.open = openProc;
    }

    if (closeProc != NULL) {
        defaultStream.close = closeProc;
    }

    if (readProc != NULL) {
        defaultStream.read = readProc;
    }

    if (writeProc != NULL) {
        defaultStream.write = writeProc;
    }

    if (seekProc != NULL) {
        defaultStream.seek = seekProc;
    }

    if (tellProc != NULL) {
        defaultStream.tell = tellProc;
    }

    if (fileLengthProc != NULL) {
        defaultStream.filelength = fileLengthProc;
    }

    soundErrorno = SOUND_NO_ERROR;
    return soundErrorno;
}
