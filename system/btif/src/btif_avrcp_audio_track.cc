/*
 * Copyright 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "bt_btif_avrcp_audio_track"

#include "btif_avrcp_audio_track.h"

#include <aaudio/AAudio.h>
#include <base/logging.h>
#include <utils/StrongPointer.h>

#include <algorithm>

#include "bt_target.h"
#include "osi/include/log.h"

using namespace android;

typedef struct {
  AAudioStream* stream;
  int bitsPerSample;
  int channelCount;
  aaudio_format_t format; // Savitech LHDC_A2DP_SINK patch
  float* buffer;
  size_t bufferLength;
  float gain;
} BtifAvrcpAudioTrack;

#if (DUMP_PCM_DATA == TRUE)
FILE* outputPcmSampleFile;
char outputFilename[50] = "/data/misc/bluedroid/output_sample.pcm";
#endif

// Maximum track gain that can be set.
constexpr float kMaxTrackGain = 1.0f;
// Minimum track gain that can be set.
constexpr float kMinTrackGain = 0.0f;

void* BtifAvrcpAudioTrackCreate(int trackFreq, int bitsPerSample,
                                int channelCount) {
  LOG_VERBOSE("%s Track.cpp: btCreateTrack freq %d bps %d channel %d ",
              __func__, trackFreq, bitsPerSample, channelCount);

  AAudioStreamBuilder* builder;
  AAudioStream* stream;
  aaudio_format_t format; // Savitech LHDC_A2DP_SINK patch
  aaudio_result_t result = AAudio_createStreamBuilder(&builder);
  AAudioStreamBuilder_setSampleRate(builder, trackFreq);
  // Savitech LHDC_A2DP_SINK patch - START
  format = AAUDIO_FORMAT_PCM_FLOAT;
  AAudioStreamBuilder_setFormat(builder, format);
  // Savitech LHDC_A2DP_SINK patch - END
  AAudioStreamBuilder_setChannelCount(builder, channelCount);
  AAudioStreamBuilder_setSessionId(builder, AAUDIO_SESSION_ID_ALLOCATE);
  AAudioStreamBuilder_setPerformanceMode(builder,
                                         AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
  result = AAudioStreamBuilder_openStream(builder, &stream);
  CHECK(result == AAUDIO_OK);
  AAudioStreamBuilder_delete(builder);

  BtifAvrcpAudioTrack* trackHolder = new BtifAvrcpAudioTrack;
  CHECK(trackHolder != NULL);
  trackHolder->stream = stream;
  trackHolder->bitsPerSample = bitsPerSample;
  trackHolder->channelCount = channelCount;
  trackHolder->format = format; // Savitech LHDC_A2DP_SINK patch
  trackHolder->bufferLength =
      trackHolder->channelCount * AAudioStream_getBufferSizeInFrames(stream);
  trackHolder->gain = kMaxTrackGain;
  trackHolder->buffer = new float[trackHolder->bufferLength]();

#if (DUMP_PCM_DATA == TRUE)
  outputPcmSampleFile = fopen(outputFilename, "ab");
#endif
  return (void*)trackHolder;
}

void BtifAvrcpAudioTrackStart(void* handle) {
  if (handle == NULL) {
    LOG_ERROR("%s: handle is null!", __func__);
    return;
  }
  BtifAvrcpAudioTrack* trackHolder = static_cast<BtifAvrcpAudioTrack*>(handle);
  CHECK(trackHolder != NULL);
  CHECK(trackHolder->stream != NULL);
  LOG_VERBOSE("%s Track.cpp: btStartTrack", __func__);
  AAudioStream_requestStart(trackHolder->stream);
}

void BtifAvrcpAudioTrackStop(void* handle) {
  if (handle == NULL) {
    LOG_INFO("%s handle is null.", __func__);
    return;
  }
  BtifAvrcpAudioTrack* trackHolder = static_cast<BtifAvrcpAudioTrack*>(handle);
  if (trackHolder != NULL && trackHolder->stream != NULL) {
    LOG_VERBOSE("%s Track.cpp: btStartTrack", __func__);
    AAudioStream_requestStop(trackHolder->stream);
  }
}

void BtifAvrcpAudioTrackDelete(void* handle) {
  if (handle == NULL) {
    LOG_INFO("%s handle is null.", __func__);
    return;
  }
  BtifAvrcpAudioTrack* trackHolder = static_cast<BtifAvrcpAudioTrack*>(handle);
  if (trackHolder != NULL && trackHolder->stream != NULL) {
    LOG_VERBOSE("%s Track.cpp: btStartTrack", __func__);
    AAudioStream_close(trackHolder->stream);
    delete trackHolder->buffer;
    delete trackHolder;
  }

#if (DUMP_PCM_DATA == TRUE)
  if (outputPcmSampleFile) {
    fclose(outputPcmSampleFile);
  }
  outputPcmSampleFile = NULL;
#endif
}

void BtifAvrcpAudioTrackPause(void* handle) {
  if (handle == NULL) {
    LOG_INFO("%s handle is null.", __func__);
    return;
  }
  BtifAvrcpAudioTrack* trackHolder = static_cast<BtifAvrcpAudioTrack*>(handle);
  if (trackHolder != NULL && trackHolder->stream != NULL) {
    LOG_VERBOSE("%s Track.cpp: btPauseTrack", __func__);
    AAudioStream_requestPause(trackHolder->stream);
    AAudioStream_requestFlush(trackHolder->stream);
  }
}

void BtifAvrcpSetAudioTrackGain(void* handle, float gain) {
  if (handle == NULL) {
    LOG_INFO("%s handle is null.", __func__);
    return;
  }
  BtifAvrcpAudioTrack* trackHolder = static_cast<BtifAvrcpAudioTrack*>(handle);
  if (trackHolder != NULL) {
    const float clampedGain = std::clamp(gain, kMinTrackGain, kMaxTrackGain);
    if (clampedGain != gain) {
      LOG_WARN("Out of bounds gain set. Clamping the gain from :%f to %f", gain,
               clampedGain);
    }
    trackHolder->gain = clampedGain;
    LOG_INFO("Avrcp audio track gain is set to %f", trackHolder->gain);
  }
}

constexpr float kScaleQ15ToFloat = 1.0f / 32768.0f;
constexpr float kScaleQ23ToFloat = 1.0f / 8388608.0f;
constexpr float kScaleQ31ToFloat = 1.0f / 2147483648.0f;

static size_t sampleSizeFor(BtifAvrcpAudioTrack* trackHolder) {
  return trackHolder->bitsPerSample / 8;
}

static size_t transcodeQ15ToFloat(uint8_t* buffer, size_t length,
                                  BtifAvrcpAudioTrack* trackHolder) {
  size_t sampleSize = sampleSizeFor(trackHolder);
  size_t i = 0;
  const float scaledGain = trackHolder->gain * kScaleQ15ToFloat;
  for (; i < std::min(trackHolder->bufferLength, length / sampleSize); i++) {
    trackHolder->buffer[i] = ((int16_t*)buffer)[i] * scaledGain;
  }
  return i * sampleSize;
}

static size_t transcodeQ23ToFloat(uint8_t* buffer, size_t length,
                                  BtifAvrcpAudioTrack* trackHolder) {
  size_t sampleSize = sampleSizeFor(trackHolder);
  size_t i = 0;
  const float scaledGain = trackHolder->gain * kScaleQ23ToFloat;
  for (; i < std::min(trackHolder->bufferLength, length / sampleSize); i++) {
    size_t offset = i * sampleSize;
    sample = sample >> 8;
    trackHolder->buffer[i] = sample * scaledGain;
  }
  return i * sampleSize;
}

static size_t transcodeQ31ToFloat(uint8_t* buffer, size_t length,
                                  BtifAvrcpAudioTrack* trackHolder) {
  size_t sampleSize = sampleSizeFor(trackHolder);
  size_t i = 0;
  const float scaledGain = trackHolder->gain * kScaleQ31ToFloat;
  for (; i < std::min(trackHolder->bufferLength, length / sampleSize); i++) {
    trackHolder->buffer[i] = ((int32_t*)buffer)[i] * scaledGain;
  }
  return i * sampleSize;
}

static size_t transcodeToPcmFloat(uint8_t* buffer, size_t length,
                                  BtifAvrcpAudioTrack* trackHolder) {
  switch (trackHolder->bitsPerSample) {
    case 16:
      return transcodeQ15ToFloat(buffer, length, trackHolder);
    case 24:
      return transcodeQ23ToFloat(buffer, length, trackHolder);
    case 32:
      return transcodeQ31ToFloat(buffer, length, trackHolder);
  }
  return -1;
}

constexpr int64_t kTimeoutNanos = 100 * 1000 * 1000;  // 100 ms

int BtifAvrcpAudioTrackWriteData(void* handle, void* audioBuffer,
                                 int bufferLength) {
  BtifAvrcpAudioTrack* trackHolder = static_cast<BtifAvrcpAudioTrack*>(handle);
  CHECK(trackHolder != NULL);
  CHECK(trackHolder->stream != NULL);
  aaudio_result_t retval = -1;
#if (DUMP_PCM_DATA == TRUE)
  if (outputPcmSampleFile) {
    fwrite((audioBuffer), 1, (size_t)bufferLength, outputPcmSampleFile);
  }
#endif

  size_t sampleSize = sampleSizeFor(trackHolder);
  int transcodedCount = 0;
  do {
    // only PCM float
    CHECK(trackHolder->format == AAUDIO_FORMAT_PCM_FLOAT);
    transcodedCount +=
        transcodeToPcmFloat(((uint8_t*)audioBuffer) + transcodedCount,
                            bufferLength - transcodedCount, trackHolder);

    retval = AAudioStream_write(
        trackHolder->stream, trackHolder->buffer,
        transcodedCount / (sampleSize * trackHolder->channelCount),
        kTimeoutNanos);
    LOG_VERBOSE("%s Track.cpp: btWriteData len = %d ret = %d", __func__,
                bufferLength, retval);
  } while (transcodedCount < bufferLength);

  return transcodedCount;
}
