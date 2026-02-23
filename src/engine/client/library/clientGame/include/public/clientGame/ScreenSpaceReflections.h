// ======================================================================
//
// ScreenSpaceReflections.h
// Copyright (c) 2024
//
// ======================================================================

#ifndef INCLUDED_ScreenSpaceReflections_H
#define INCLUDED_ScreenSpaceReflections_H

// ======================================================================

class ScreenSpaceReflections
{
public:
        static void install();

        static bool isSupported();
        static bool isEnabled();
        static void setEnabled(bool enabled);

        static void preSceneRender();
        static void postSceneRender();

private:
        static void remove();
};

// ======================================================================

#endif // INCLUDED_ScreenSpaceReflections_H

