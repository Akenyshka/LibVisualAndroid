/* Libvisual - The audio visualisation framework.
 * 
 * Copyright (C) 2004-2006 Dennis Smit <ds@nerds-incorporated.org>
 * Copyright (C) 2012 Daniel Hiepler <daniel-lva@niftylight.de>         
 *
 * Authors: Dennis Smit <ds@nerds-incorporated.org>
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
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <jni.h>
#include <android/log.h>
#include <android/bitmap.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <libvisual/libvisual.h>
#include "visual.h"



/** local variables */
static struct
{

}_l;



/** VisLog -> android Log glue */
static void _log_handler(VisLogSeverity severity, const char *msg, const VisLogSource *source, void *priv)
{

    switch(severity)
    {
        case VISUAL_LOG_DEBUG:
            LOGI("(debug) %s(): %s", source->func, msg);
            break;
        case VISUAL_LOG_INFO:
            LOGI("(info) %s", msg);
            break;
        case VISUAL_LOG_WARNING:
            LOGW("(WARNING) %s", msg);
            break;
        case VISUAL_LOG_ERROR:
            LOGE("(ERROR) (%s:%d) %s(): %s", source->file, source->line, source->func, msg);
            break;
        case VISUAL_LOG_CRITICAL:
            LOGE("(CRITICAL) (%s:%d) %s(): %s", source->file, source->line, source->func, msg);
            break;
    }
}


/******************************************************************************
 ******************************************************************************/

/** LibVisual.init() */
JNIEXPORT jboolean JNICALL Java_org_libvisual_android_LibVisual_init(JNIEnv * env, jobject  obj)
{
    if(visual_is_initialized())
                return JNI_TRUE;

    LOGI("LibVisual.init(): %s", visual_get_version());

#ifndef NDEBUG
    /* endless loop to wait for debugger to attach */
    int foo = 1;
    while(foo);
#endif
        
    /* register VisLog handler to make it log to android logcat */
    visual_log_set_handler(VISUAL_LOG_DEBUG, _log_handler, NULL);
    visual_log_set_handler(VISUAL_LOG_INFO, _log_handler, NULL);
    visual_log_set_handler(VISUAL_LOG_WARNING, _log_handler, NULL);
    visual_log_set_handler(VISUAL_LOG_CRITICAL, _log_handler, NULL);
    visual_log_set_handler(VISUAL_LOG_ERROR, _log_handler, NULL);
    visual_log_set_verbosity(VISUAL_LOG_DEBUG);


    /* initialize libvisual */
    char *v[] = { "lvclient", NULL };
    char **argv = v;
    int argc=1;
    visual_init(&argc,  &argv);

    /* add our plugin search path */
    visual_plugin_registry_add_path("/data/data/org.libvisual.android/lib");

        
    return JNI_TRUE;
}


/** LibVisual.deinit() */
JNIEXPORT void JNICALL Java_org_libvisual_android_LibVisual_deinit(JNIEnv * env, jobject  obj)
{
    LOGI("LibVisual.deinit()");
        
    if(visual_is_initialized())
        visual_quit();
}

/******************************************************************************/

/** VisActor.actorNew() */
JNIEXPORT jint JNICALL Java_org_libvisual_android_VisActor_actorNew(JNIEnv * env, jobject  obj, jstring name)
{
    LOGI("VisActor.actorNew()");


    /* result */
    VisActor *a = NULL;

    /* get name string */
    jboolean isCopy;  
    const char *actorName = (*env)->GetStringUTFChars(env, name, &isCopy);  

    /* actor valid ? */
    if(!(visual_plugin_registry_has_plugin(VISUAL_PLUGIN_TYPE_ACTOR, actorName)))
    {
            LOGE("Invalid actor-plugin: \"%s\"", actorName);
            goto _van_exit;
    }

    /* create new actor */
    a = visual_actor_new(actorName);

    /* set random seed */
    VisPluginData    *plugin_data = visual_actor_get_plugin(a);
    VisRandomContext *r_context   = visual_plugin_get_random_context (plugin_data);
    visual_random_context_set_seed(r_context, time(NULL));

_van_exit:
    (*env)->ReleaseStringUTFChars(env, name, actorName);
    return (jint) a;
}


/** VisActor.actorUnref() */
JNIEXPORT void JNICALL Java_org_libvisual_android_VisActor_actorUnref(JNIEnv * env, jobject  obj, jint actor)
{
    LOGI("VisActor.actorUnref()");

    VisActor *a = (VisActor *) actor;
    visual_object_unref(VISUAL_OBJECT(actor));
}


/** VisActor.actorGetSupportedDepth() */
JNIEXPORT jint JNICALL Java_org_libvisual_android_VisActor_actorGetSupportedDepth(JNIEnv * env, jobject  obj, jint actor)
{
    VisActor *a = (VisActor *) actor;
    return visual_actor_get_supported_depth(a);
}

/** VisActor.actorVideoNegotiate() */
JNIEXPORT jint JNICALL Java_org_libvisual_android_VisActor_actorVideoNegotiate(JNIEnv * env, jobject  obj, jint actor, jint rundepth, jboolean noevent, jboolean forced)
{
    VisActor *a = (VisActor *) actor;
            
    return visual_actor_video_negotiate(a, rundepth, noevent, forced);
}

/** VisActor.actorGetPlugin() */
JNIEXPORT jint JNICALL Java_org_libvisual_android_VisActor_actorGetPlugin(JNIEnv * env, jobject  obj, jint actor)
{
    VisActor *a = (VisActor *) actor;
    return (jint) visual_actor_get_plugin(a);
}

/******************************************************************************/

/** VisInput.inputNew() */
JNIEXPORT jint JNICALL Java_org_libvisual_android_VisInput_inputNew(JNIEnv * env, jobject  obj, jstring name)
{
    LOGI("VisInput.inputNew()");

    /* result */
    VisInput *i = NULL;
        
    /* get name string */
    jboolean isCopy;  
    const char *inputName = (*env)->GetStringUTFChars(env, name, &isCopy);  

    /* plugin valid ? */
    if(!(visual_plugin_registry_has_plugin(VISUAL_PLUGIN_TYPE_INPUT, inputName)))
    {
            LOGE("Invalid input-plugin: \"%s\"", inputName);
            goto _vin_exit;
    }

    /* create new input */
    i = visual_input_new(inputName);
        
_vin_exit:
    (*env)->ReleaseStringUTFChars(env, name, inputName);
    return (jint) i;
}


/** VisInput.inputUnref() */
JNIEXPORT void JNICALL Java_org_libvisual_android_VisInput_inputUnref(JNIEnv * env, jobject  obj, jint input)
{
    LOGI("VisInput.inputUnref()");

    VisInput *i = (VisInput *) input;
    visual_object_unref(VISUAL_OBJECT(input));        
}


/** VisActor.inputGetPlugin() */
JNIEXPORT jint JNICALL Java_org_libvisual_android_VisInput_inputGetPlugin(JNIEnv * env, jobject  obj, jint input)
{
    VisInput *i = (VisInput *) input;
    return (jint) visual_input_get_plugin(i);
}


/******************************************************************************/

/** VisMorph.morphNew() */
JNIEXPORT jint JNICALL Java_org_libvisual_android_VisMorph_morphNew(JNIEnv * env, jobject  obj, jstring name)
{
    LOGI("VisMorph.morphNew()");

    /* result */
    VisMorph *m = NULL;
        
    /* get name string */
    jboolean isCopy;  
    const char *morphName = (*env)->GetStringUTFChars(env, name, &isCopy);  

    /* plugin valid ? */
    if(!(visual_plugin_registry_has_plugin(VISUAL_PLUGIN_TYPE_MORPH, morphName)))
    {
            LOGE("Invalid morph-plugin: \"%s\"", morphName);
            goto _vin_exit;
    }

    /* create new morph */
    m = visual_morph_new(morphName);
        
_vin_exit:
    (*env)->ReleaseStringUTFChars(env, name, morphName);
    return (jint) m;
}


/** VisMorph.morphUnref() */
JNIEXPORT void JNICALL Java_org_libvisual_android_VisMorph_morphUnref(JNIEnv * env, jobject  obj, jint morph)
{
    LOGI("VisMorph.morphUnref()");

    VisMorph *m = (VisMorph *) morph;
    visual_object_unref(VISUAL_OBJECT(morph));        
}

/** VisActor.morphGetPlugin() */
JNIEXPORT jint JNICALL Java_org_libvisual_android_VisMorph_morphGetPlugin(JNIEnv * env, jobject  obj, jint morph)
{
    VisMorph *m = (VisMorph *) morph;
    return (jint) visual_morph_get_plugin(m);
}


/******************************************************************************/

/** VisBin.binNew() */
JNIEXPORT jint JNICALL Java_org_libvisual_android_VisBin_binNew(JNIEnv * env, jobject  obj)
{
    LOGI("VisBin.binNew()");

    return (jint) visual_bin_new();
}


/** VisBin.binUnref() */
JNIEXPORT void JNICALL Java_org_libvisual_android_VisBin_binUnref(JNIEnv * env, jobject  obj, jint bin)
{
    LOGI("VisBin.binUnref()");

    VisBin *b = (VisBin *) bin;
    visual_object_unref(VISUAL_OBJECT(bin));        
}


/** VisBin.setDepth() */
JNIEXPORT void JNICALL Java_org_libvisual_android_VisBin_binSetDepth(JNIEnv * env, jobject  obj, jint bin, jint depth)
{    
    VisBin *b = (VisBin *) bin;
    visual_bin_set_depth(b, depth);
}


/** VisBin.setSupportedDepth() */
JNIEXPORT void JNICALL Java_org_libvisual_android_VisBin_binSetSupportedDepth(JNIEnv * env, jobject  obj, jint bin, jint depth)
{    
    VisBin *b = (VisBin *) bin;
    visual_bin_set_supported_depth(b, depth);
}


/** VisBin.setSupportedDepth() */
JNIEXPORT void JNICALL Java_org_libvisual_android_VisBin_binSetPreferredDepth(JNIEnv * env, jobject  obj, jint bin, jint depth)
{    
    VisBin *b = (VisBin *) bin;
    visual_bin_set_preferred_depth(b, depth);
}


/** VisBin.setVideo() */
JNIEXPORT void JNICALL Java_org_libvisual_android_VisBin_binSetVideo(JNIEnv * env, jobject  obj, jint bin, jint video)
{    
    VisBin *b = (VisBin *) bin;
    VisVideo *v = (VisVideo *) video;
    visual_bin_set_video(b, v);
}


/** VisBin.realize() */
JNIEXPORT void JNICALL Java_org_libvisual_android_VisBin_binRealize(JNIEnv * env, jobject  obj, jint bin)
{    
    VisBin *b = (VisBin *) bin;
    visual_bin_realize(b);
}


/** VisBin.sync() */
JNIEXPORT void JNICALL Java_org_libvisual_android_VisBin_binSync(JNIEnv * env, jobject  obj, jint bin, jboolean noevent)
{    
    VisBin *b = (VisBin *) bin;
    visual_bin_sync(b, noevent);
}


/** VisBin.depthChanged() */
JNIEXPORT void JNICALL Java_org_libvisual_android_VisBin_binDepthChanged(JNIEnv * env, jobject  obj, jint bin)
{    
    VisBin *b = (VisBin *) bin;
    visual_bin_depth_changed(b);
}


/** VisBin.connect() */
JNIEXPORT void JNICALL Java_org_libvisual_android_VisBin_binConnect(JNIEnv * env, jobject  obj, jint bin, jint actor, jint input)
{    
    VisBin *b = (VisBin *) bin;
    VisActor *a = (VisActor *) actor;
    VisInput *i = (VisInput *) input;
    visual_bin_connect(b, a, i);
}


/** VisBin.setMorphByName() */
JNIEXPORT jint JNICALL Java_org_libvisual_android_VisBin_binSetMorphByName(JNIEnv * env, jobject  obj, jint bin, jstring name)
{    
    jboolean isCopy;  
    const char *n = (*env)->GetStringUTFChars(env, name, &isCopy);  
        
    VisBin *b = (VisBin *) bin;
    int r = visual_bin_set_morph_by_name(b, n);

    (*env)->ReleaseStringUTFChars(env, name, n);

    return r;
}


/** VisBin.switchActor() */
JNIEXPORT jint JNICALL Java_org_libvisual_android_VisBin_binSwitchActorByName(JNIEnv * env, jobject  obj, jint bin, jstring name)
{
    jboolean isCopy;  
    const char *n = (*env)->GetStringUTFChars(env, name, &isCopy);  

    VisBin *b = (VisBin *) bin;
    int r = visual_bin_switch_actor_by_name(b, n);

    (*env)->ReleaseStringUTFChars(env, name, n);

    return r;
}


/** VisBin.getMorph() */
JNIEXPORT jint JNICALL Java_org_libvisual_android_VisBin_binGetMorph(JNIEnv * env, jobject  obj, jint bin)
{
    VisBin *b = (VisBin *) bin;
        
    return (jint) visual_bin_get_morph(b);
}


/** VisBin.getActor() */
JNIEXPORT jint JNICALL Java_org_libvisual_android_VisBin_binGetActor(JNIEnv * env, jobject  obj, jint bin)
{
    VisBin *b = (VisBin *) bin;

    return (jint) visual_bin_get_actor(b);
}


/******************************************************************************/

/** VisVideo.videoNew() */
JNIEXPORT jint JNICALL Java_org_libvisual_android_VisVideo_videoNew(JNIEnv * env, jobject  obj)
{
    LOGI("VisVideo.videoNew()");

    return (jint) visual_video_new();
}


/** VisVideo.videoUnref() */
JNIEXPORT void JNICALL Java_org_libvisual_android_VisVideo_videoUnref(JNIEnv * env, jobject  obj, jint video)
{
    LOGI("VisVideo.videoUnref()");

    VisVideo *v = (VisVideo *) video;
    visual_object_unref(VISUAL_OBJECT(video));        
}


/** VisVideo.setAttributes() */
JNIEXPORT void JNICALL Java_org_libvisual_android_VisVideo_videoSetAttributes(JNIEnv * env, 
                                                                              jobject  obj, 
                                                                              jint video, 
                                                                              jint width, 
                                                                              jint height, 
                                                                              jint stride, 
                                                                              jint depth)
{
    LOGI("VisVideo.videoSetAttributes()");
        
    VisVideo *v = (VisVideo *) video;
    visual_video_set_attributes(v, width, height, stride, depth);
}


/** VisVideo.depthGetHighest() */
JNIEXPORT jint JNICALL Java_org_libvisual_android_VisVideo_videoGetHighestDepth(JNIEnv * env, jobject  obj, jint depth)
{
    LOGI("VisVideo.videoGetHighestDepth()");

    return visual_video_depth_get_highest(depth);
}


/** VisVideo.depthGetHighestNoGl() */
JNIEXPORT jint JNICALL Java_org_libvisual_android_VisVideo_videoGetHighestDepthNoGl(JNIEnv * env, jobject  obj, jint depth)
{
    LOGI("VisVideo.videoGetHighestDepthNoGl()");

    return visual_video_depth_get_highest_nogl(depth);
}


/** VisVideo.depthGetHighestNoGl() */
JNIEXPORT jint JNICALL Java_org_libvisual_android_VisVideo_videoBppFromDepth(JNIEnv * env, jobject  obj, jint depth)
{
    LOGI("VisVideo.videoBppFromDepth()");
    return visual_video_bpp_from_depth(depth);
}


/** VisVideo.videoAllocateBuffer() */
JNIEXPORT void JNICALL Java_org_libvisual_android_VisVideo_videoAllocateBuffer(JNIEnv * env, jobject  obj, int videoPtr)
{
    LOGI("VisVideo.videoAllocateBuffer()");
    VisVideo *v = (VisVideo *) videoPtr;
    visual_video_allocate_buffer(v);
}



/******************************************************************************/

/** VisPlugin.pluginGetName() */
JNIEXPORT jstring JNICALL Java_org_libvisual_android_VisPlugin_pluginGetName(JNIEnv * env, jobject  obj, int pluginPtr)
{
    VisPluginData *d = (VisPluginData *) pluginPtr;
        
    return (*env)->NewStringUTF(env, d->info->name);
}

/** VisPlugin.pluginGetPlugname() */
JNIEXPORT jstring JNICALL Java_org_libvisual_android_VisPlugin_pluginGetPlugname(JNIEnv * env, jobject  obj, int pluginPtr)
{
    VisPluginData *d = (VisPluginData *) pluginPtr;
        
    return (*env)->NewStringUTF(env, d->info->plugname);
}

/** VisPlugin.pluginGetAuthor() */
JNIEXPORT jstring JNICALL Java_org_libvisual_android_VisPlugin_pluginGetAuthor(JNIEnv * env, jobject  obj, int pluginPtr)
{
    VisPluginData *d = (VisPluginData *) pluginPtr;
        
    return (*env)->NewStringUTF(env, d->info->author);
}

/** VisPlugin.pluginGetVersion() */
JNIEXPORT jstring JNICALL Java_org_libvisual_android_VisPlugin_pluginGetVersion(JNIEnv * env, jobject  obj, int pluginPtr)
{
    VisPluginData *d = (VisPluginData *) pluginPtr;
        
    return (*env)->NewStringUTF(env, d->info->version);
}

/** VisPlugin.pluginGetAbout() */
JNIEXPORT jstring JNICALL Java_org_libvisual_android_VisPlugin_pluginGetAbout(JNIEnv * env, jobject  obj, int pluginPtr)
{
    VisPluginData *d = (VisPluginData *) pluginPtr;
        
    return (*env)->NewStringUTF(env, d->info->about);
}

/** VisPlugin.pluginGetHelp() */
JNIEXPORT jstring JNICALL Java_org_libvisual_android_VisPlugin_pluginGetHelp(JNIEnv * env, jobject  obj, int pluginPtr)
{
    VisPluginData *d = (VisPluginData *) pluginPtr;
        
    return (*env)->NewStringUTF(env, d->info->help);
}
    
/** VisPlugin.pluginGetLicense() */
JNIEXPORT jstring JNICALL Java_org_libvisual_android_VisPlugin_pluginGetLicense(JNIEnv * env, jobject  obj, int pluginPtr)
{
    VisPluginData *d = (VisPluginData *) pluginPtr;
        
    return (*env)->NewStringUTF(env, d->info->license);
}
