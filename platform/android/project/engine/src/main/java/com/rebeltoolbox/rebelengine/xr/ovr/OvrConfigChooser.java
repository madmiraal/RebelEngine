// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

package com.rebeltoolbox.rebelengine.xr.ovr;

import android.opengl.EGLExt;
import android.opengl.GLSurfaceView;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLDisplay;

/**
 * EGL config chooser for the Oculus Mobile VR SDK.
 */
public class OvrConfigChooser implements GLSurfaceView.EGLConfigChooser {
    private static final int[] CONFIG_ATTRIBS = {
        EGL10.EGL_RED_SIZE,
        8,
        EGL10.EGL_GREEN_SIZE,
        8,
        EGL10.EGL_BLUE_SIZE,
        8,
        EGL10.EGL_ALPHA_SIZE,
        8, // Need alpha for the multi-pass timewarp compositor
        EGL10.EGL_DEPTH_SIZE,
        0,
        EGL10.EGL_STENCIL_SIZE,
        0,
        EGL10.EGL_SAMPLES,
        0,
        EGL10.EGL_NONE
    };

    @Override
    public EGLConfig chooseConfig(EGL10 egl, EGLDisplay display) {
        // Do NOT use eglChooseConfig, because the Android EGL code pushes in
        // multisample flags in eglChooseConfig if the user has selected the
        // "force 4x MSAA" option in settings, and that is completely wasted for
        // our warp target.
        int[] numConfig = new int[1];
        if (!egl.eglGetConfigs(display, null, 0, numConfig)) {
            throw new IllegalArgumentException("eglGetConfigs failed.");
        }

        int configsCount = numConfig[0];
        if (configsCount <= 0) {
            throw new IllegalArgumentException("No configs match configSpec");
        }

        EGLConfig[] configs = new EGLConfig[configsCount];
        if (!egl.eglGetConfigs(display, configs, configsCount, numConfig)) {
            throw new IllegalArgumentException("eglGetConfigs #2 failed.");
        }

        int[] value = new int[1];
        for (EGLConfig config : configs) {
            egl.eglGetConfigAttrib(
                display,
                config,
                EGL10.EGL_RENDERABLE_TYPE,
                value
            );
            if ((value[0] & EGLExt.EGL_OPENGL_ES3_BIT_KHR)
                != EGLExt.EGL_OPENGL_ES3_BIT_KHR) {
                continue;
            }

            // The pbuffer config also needs to be compatible with normal window
            // rendering so it can share textures with the window context.
            egl.eglGetConfigAttrib(
                display,
                config,
                EGL10.EGL_SURFACE_TYPE,
                value
            );
            if ((value[0] & (EGL10.EGL_WINDOW_BIT | EGL10.EGL_PBUFFER_BIT))
                != (EGL10.EGL_WINDOW_BIT | EGL10.EGL_PBUFFER_BIT)) {
                continue;
            }

            // Check each attribute in CONFIG_ATTRIBS (which are the attributes
            // we care about) and ensure the value in config matches.
            int attribIndex = 0;
            while (CONFIG_ATTRIBS[attribIndex] != EGL10.EGL_NONE) {
                egl.eglGetConfigAttrib(
                    display,
                    config,
                    CONFIG_ATTRIBS[attribIndex],
                    value
                );
                if (value[0] != CONFIG_ATTRIBS[attribIndex + 1]) {
                    // Attribute key's value does not match the configs value.
                    // Start checking next config.
                    break;
                }

                // Step by two because CONFIG_ATTRIBS is in key/value pairs.
                attribIndex += 2;
            }

            if (CONFIG_ATTRIBS[attribIndex] == EGL10.EGL_NONE) {
                // All relevant attributes match, set the config and stop
                // checking the rest.
                return config;
            }
        }
        return null;
    }
}