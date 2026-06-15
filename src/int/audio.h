#ifndef FALLOUT_INT_AUDIO_H_
#define FALLOUT_INT_AUDIO_H_

#include <stdint.h>

#include "int/audiof.h"

intptr_t audioOpen(const char* fname, int mode, ...);
int audioCloseFile(intptr_t fileHandle);
int audioRead(intptr_t fileHandle, void* buffer, unsigned int size);
long audioSeek(intptr_t fileHandle, long offset, int origin);
long audioFileSize(intptr_t fileHandle);
long audioTell(intptr_t fileHandle);
int audioWrite(intptr_t handle, const void* buf, unsigned int size);
int initAudio(AudioFileIsCompressedProc* isCompressedProc);
void audioClose();

#endif /* FALLOUT_INT_AUDIO_H_ */
