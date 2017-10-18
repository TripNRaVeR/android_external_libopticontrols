/*
 * Copyright (C) 2017 TripNDroid Mobile Engineering
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

#include <jni.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "soundmod.h"
#include "samplerate.h"

SF_INFO sfiIRInfo;
SNDFILE *sfIRFile;

JNIEXPORT jintArray JNICALL Java_optisound_activity_opticontrols_GetLoadImpulseResponseInfo (JNIEnv *env, jobject obj, jstring path) {

    const char *mIRFileName = (*env)->GetStringUTFChars(env, path, 0);

    if (strlen(mIRFileName) <= 0) return 0;

    jint jImpInfo[4] = { 0, 0, 0, 0 };
    jintArray jrImpInfo = (*env)->NewIntArray(env, 4);

    if (!jrImpInfo) return 0;

    memset(&sfiIRInfo, 0, sizeof(SF_INFO));
    sfIRFile = sf_open(mIRFileName, SFM_READ, &sfiIRInfo);

    if (sfIRFile == NULL) {
        return 0;
    }
    if ((sfiIRInfo.channels != 1) && (sfiIRInfo.channels != 2) && (sfiIRInfo.channels != 4)) {
        sf_close(sfIRFile);
        return 0;
    }
    if ((sfiIRInfo.samplerate <= 0) || (sfiIRInfo.frames <= 0)) {
        sf_close(sfIRFile);
        return 0;
    }

    jImpInfo[0] = (jint)sfiIRInfo.channels;
    jImpInfo[1] = (jint)sfiIRInfo.frames;
    jImpInfo[2] = (jint)sfiIRInfo.samplerate;
    jImpInfo[3] = (jint)sfiIRInfo.format;

    (*env)->SetIntArrayRegion(env, jrImpInfo, 0, 4, jImpInfo);
    (*env)->ReleaseStringUTFChars(env, path, mIRFileName);

    return jrImpInfo;
}

JNIEXPORT jintArray JNICALL Java_optisound_activity_opticontrols_ReadImpulseResponseToInt (JNIEnv *env, jobject obj, jint targetSampleRate) {

    jintArray outbuf;
    int i;
    int *final;
    int frameCountTotal = sfiIRInfo.channels * sfiIRInfo.frames;
    float *pFrameBuffer = (float*)malloc(frameCountTotal * sizeof(float));

    if (!pFrameBuffer) {
        sf_close(sfIRFile);
        return 0;
    }

    sf_readf_float(sfIRFile, pFrameBuffer, sfiIRInfo.frames);
    sf_close(sfIRFile);

    if (sfiIRInfo.samplerate == targetSampleRate) {
        final = (int*)malloc(frameCountTotal * sizeof(int));

        for (i = 0; i < frameCountTotal; i++)
            final[i] = pFrameBuffer[i] * 32768.0f;

        outbuf = (*env)->NewIntArray(env, (jsize)frameCountTotal);
        (*env)->SetIntArrayRegion(env, outbuf, 0, (jsize)frameCountTotal, final);
        free(final);
    }
    else {

        double convertionRatio = (double)targetSampleRate / (double)sfiIRInfo.samplerate;
        int resampledframeCountTotal = (int)((double)frameCountTotal * convertionRatio);
        int outFramesPerChannel = (int)((double)sfiIRInfo.frames * convertionRatio);
        float *out = (float*)malloc(resampledframeCountTotal * sizeof(float));
        int error;
        SRC_DATA data;

        data.data_in = pFrameBuffer;
        data.data_out = out;
        data.input_frames = sfiIRInfo.frames;
        data.output_frames = outFramesPerChannel;
        data.src_ratio = convertionRatio;

        error = src_simple(&data, 3, sfiIRInfo.channels);
        unsigned int finalOut = resampledframeCountTotal;
        final = (int*)malloc(finalOut * sizeof(int));
        for (i = 0; i < finalOut; i++)
            final[i] = out[i] * 32768.0f;
        jsize jsFrameBufferSize = finalOut;
        outbuf = (*env)->NewIntArray(env, jsFrameBufferSize);
        (*env)->SetIntArrayRegion(env, outbuf, 0, jsFrameBufferSize, final);

        free(out);
        free(final);
    }
    free(pFrameBuffer);
    return outbuf;
}

JNIEXPORT jstring JNICALL Java_optisound_activity_opticontrols_OfflineAudioResample
(JNIEnv *env, jobject obj, jstring path, jstring filename, jint targetSampleRate)
{
    SF_INFO sfinfo;
    SNDFILE *infile, *outfile;
    sf_count_t count;
    const char *jnipath = (*env)->GetStringUTFChars(env, path, 0);
    if (strlen(jnipath) <= 0) return 0;
    const char *mIRFileName = (*env)->GetStringUTFChars(env, filename, 0);
    if (strlen(mIRFileName) <= 0) return 0;
    memset(&sfinfo, 0, sizeof(SF_INFO));
    size_t needed = snprintf(NULL, 0, "%s%s", jnipath, mIRFileName) + 1;
    char *filenameIR = malloc(needed);
    snprintf(filenameIR, needed, "%s%s", jnipath, mIRFileName);
    infile = sf_open(filenameIR, SFM_READ, &sfinfo);
    free(filenameIR);
    if (infile == NULL) {
        return NULL;
    }
    if ((sfinfo.channels != 1) && (sfinfo.channels != 2) && (sfinfo.channels != 4)) {
        sf_close(infile);
        return NULL;
    }
    if ((sfinfo.samplerate <= 0) || (sfinfo.frames <= 0)) {
        sf_close(infile);
        return NULL;
    }

    double src_ratio = (double)targetSampleRate / (double)sfinfo.samplerate;
    needed = snprintf(NULL, 0, "%s%d_%s", jnipath, targetSampleRate, mIRFileName) + 1;
    filenameIR = malloc(needed);
    snprintf(filenameIR, needed, "%s%d_%s", jnipath, targetSampleRate, mIRFileName);

    int frameCountTotal = sfinfo.channels * sfinfo.frames;
    int resampledframeCountTotal = (int)((double)frameCountTotal * src_ratio);
    int outFramesPerChannel = (int)((double)sfinfo.frames * src_ratio);

    float *pFrameBuffer = (float*)malloc(frameCountTotal * sizeof(float));
    if (!pFrameBuffer) {
        sf_close(infile);
        return NULL;
    }

    sf_readf_float(infile, pFrameBuffer, sfinfo.frames);
    sf_close(infile);

    float *out = (float*)calloc(resampledframeCountTotal, sizeof(float));
    int error;

    SRC_DATA data;
    data.data_in = pFrameBuffer;
    data.data_out = out;
    data.input_frames = sfinfo.frames;
    data.output_frames = outFramesPerChannel;
    data.src_ratio = src_ratio;
    error = src_simple(&data, 0, sfinfo.channels);
    free(pFrameBuffer);
    sfinfo.frames = outFramesPerChannel;
    sfinfo.samplerate = targetSampleRate;
    outfile = sf_open(filenameIR, SFM_WRITE, &sfinfo);
    sf_writef_float(outfile, out, outFramesPerChannel);
    sf_close(outfile);
    free(out);
    (*env)->ReleaseStringUTFChars(env, path, jnipath);
    (*env)->ReleaseStringUTFChars(env, filename, mIRFileName);
    jstring finalName = (*env)->NewStringUTF(env, filenameIR);

    free(filenameIR);
    return finalName;
}
