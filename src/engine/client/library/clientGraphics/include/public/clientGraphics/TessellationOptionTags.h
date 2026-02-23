// ======================================================================
//
// TessellationOptionTags.h
//
// Copyright 2024 Sony Online Entertainment
// All Rights Reserved.
//
// ======================================================================

#ifndef INCLUDED_TessellationOptionTags_H
#define INCLUDED_TessellationOptionTags_H

// ======================================================================

#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/Tag.h"

// ======================================================================

namespace TessellationOptionTags
{
        // Terrain and character specific tessellation option tags used to gate
        // hull/domain shader variants in the shader template system.
        inline Tag getTerrainTag()
        {
                return TAG(T,T,R,N);
        }

        inline Tag getCharacterTag()
        {
                return TAG(C,T,E,S);
        }
}

// ======================================================================

#endif
