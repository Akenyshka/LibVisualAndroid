/* Libvisual-plugins - Standard plugins for libvisual
 * 
 * Copyright (C) 2012
 *
 * Authors: Scott Sibley <sisibley@gmail.com>
 *          Daniel Hiepler <daniel-lva@niftylight.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef _INPUT_ANDROID_AUDIO_RECORD_H
#define _INPUT_ANDROID_AUDIO_RECORD_H


#include <jni.h>

/* AudioSource */
typedef enum
{
    DEFAULT = 0,
    MIC,
    VOICE_UPLINK,
    VOICE_DOWNLINK,
    VOICE_CALL,
    CAMCORDER,
    VOICE_RECOGNITION,
    VOICE_COMMUNICATION,
}AudioSource;

/* AudioFormat */
#define	CHANNEL_IN_MONO     (0x10)
#define ENCODING_PCM_16BIT  (0x02)
#define ENCODING_PCM_8BIT   (0x03)





typedef jobject AudioRecordJNI;



AudioRecordJNI      AudioRecord(jint audioSource, jint sampleRateInHz, jint channelConfig, jint audioFormat, jint bufferSizeInBytes);



#endif /* _INPUT_ANDROID_AUDIO_RECORD_H */
