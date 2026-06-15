#ifndef FALLOUT_INT_AUDIOF_H_
#define FALLOUT_INT_AUDIOF_H_

#include <stdbool.h>
#include <stdint.h>

#include "sound_decoder.h"

typedef enum AudioFileFlags {
    AUDIO_FILE_IN_USE = 0x01,
    AUDIO_FILE_COMPRESSED = 0x02,
} AudioFileFlags;

typedef struct AudioFile {
    int flags;
    intptr_t fileHandle;
    SoundDecoder* soundDecoder;
    int fileSize;
    int field_10;
    int field_14;
    int position;
} AudioFile;

typedef bool(AudioFileIsCompressedProc)(char* filePath);

intptr_t audiofOpen(const char* fname, int flags, ...);
int audiofCloseFile(intptr_t a1);
int audiofRead(intptr_t a1, void* buf, unsigned int size);
long audiofSeek(intptr_t handle, long offset, int origin);
long audiofFileSize(intptr_t a1);
long audiofTell(intptr_t a1);
int audiofWrite(intptr_t handle, const void* buf, unsigned int size);
int initAudiof(AudioFileIsCompressedProc* isCompressedProc);
void audiofClose();

#endif /* FALLOUT_INT_AUDIOF_H_ */
