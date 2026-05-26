#pragma once

#include "GameClientLite.h"

#include <stdint.h>

namespace AwesomeMacroLite {
    struct UnitInfoLite {
        void* unit;
        void* nameplate;
        guid_t guid;
        uint32_t flags;
        uint32_t npcFlags;
        int rank;
        int reaction;
        bool canAttack;
        bool canAssist;
        bool friendly;
        bool enemy;
    };

    bool TryGetUnitInfo(void* unit, UnitInfoLite& info);
    bool TryGetUnitInfoByGuid(guid_t guid, UnitInfoLite& info);
}
