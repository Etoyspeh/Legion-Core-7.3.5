/*
 * Copyright (C) 2005-2011 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef TRINITYSERVER_MOVESPLINEFLAG_H
#define TRINITYSERVER_MOVESPLINEFLAG_H

#include "MovementTypedefs.h"

namespace Movement
{
#pragma pack(push, 1)

    class MoveSplineFlag
    {
    public:
        enum eFlags
        {
            None                = 0x00000000,
            Unknown_0x1         = 0x00000001,           // NOT VERIFIED
            Unknown_0x2         = 0x00000002,           // NOT VERIFIED
            Unknown_0x4         = 0x00000004,           // NOT VERIFIED
            Unknown_0x8         = 0x00000008,           // NOT VERIFIED - does someting related to falling/fixed orientation
            FallingSlow         = 0x00000010,
            Done                = 0x00000020,
            Falling             = 0x00000040,           // Affects elevation computation, can't be combined with Parabolic flag
            No_Spline           = 0x00000080,
            Unknown1            = 0x00000100,           // NOT VERIFIED
            Flying              = 0x00000200,           // Smooth movement(Catmullrom interpolation mode), flying animation
            OrientationFixed    = 0x00000400,           // Model orientation fixed
            Catmullrom          = 0x00000800,           // Used Catmullrom interpolation mode
            Cyclic              = 0x00001000,           // Movement by cycled spline
            Enter_Cycle         = 0x00002000,           // Everytimes appears with cyclic flag in monster move packet, erases first spline vertex after first cycle done
            Frozen              = 0x00004000,           // Will never arrive
            TransportEnter      = 0x00008000,
            TransportExit       = 0x00010000,
            Unknown2            = 0x00020000,           // NOT VERIFIED
            Unknown3            = 0x00040000,           // NOT VERIFIED
            Backward            = 0x00080000,
            SmoothGroundPath    = 0x00100000,
            CanSwim             = 0x00200000,
            UncompressedPath    = 0x00400000,
            Unknown4            = 0x00800000,           // NOT VERIFIED
            Unknown5            = 0x01000000,           // NOT VERIFIED
            Animation           = 0x02000000,           // Plays animation after some time passed
            Parabolic           = 0x04000000,           // Affects elevation computation, can't be combined with Falling flag
            FadeObject          = 0x08000000,
            Steering            = 0x10000000,
            Unknown8            = 0x20000000,           // NOT VERIFIED
            Unknown9            = 0x40000000,           // NOT VERIFIED
            Unknown10           = 0x80000000,           // NOT VERIFIED

            // Masks

            // flags that shouldn't be appended into SMSG_MONSTER_MOVE\SMSG_MONSTER_MOVE_TRANSPORT packet, should be more probably
            Mask_No_Monster_Move = Done,
        };

        uint32& raw() { return reinterpret_cast<uint32&>(*this); }

        const uint32& raw() const { return reinterpret_cast<const uint32&>(*this); }

        MoveSplineFlag() { raw() = 0; }
        MoveSplineFlag(uint32 f) { raw() = f; }
        MoveSplineFlag(const MoveSplineFlag& f) { raw() = f.raw(); }

        // Constant interface

        bool isSmooth() const { return (raw() & Catmullrom) != 0; }
        bool isLinear() const { return !isSmooth(); }

        bool hasAllFlags(uint32 f) const { return (raw() & f) == f; }
        bool hasFlag(uint32 f) const { return (raw() & f) != 0; }
        uint32 operator & (uint32 f) const { return (raw() & f); }
        uint32 operator | (uint32 f) const { return (raw() | f); }
        std::string ToString() const;

        // Not constant interface

        void operator &= (uint32 f) { raw() &= f; }
        void operator |= (uint32 f) { raw() |= f; }

        void EnableAnimation() { raw() = (raw() & ~(Falling | Parabolic | FallingSlow | FadeObject)) | Animation; }
        void EnableParabolic() { raw() = (raw() & ~(Falling | Animation | FallingSlow | FadeObject)) | Parabolic; }
        void EnableFlying() { raw() = (raw() & ~(Falling)) | Flying; }
        void EnableFalling() { raw() = (raw() & ~(Parabolic | Animation | Flying)) | Falling; }
        void EnableCatmullRom() { raw() = (raw() & ~SmoothGroundPath) | Catmullrom; }
        void EnableTransportEnter() { raw() = (raw() & ~TransportExit) | TransportEnter; }
        void EnableTransportExit() { raw() = (raw() & ~TransportEnter) | TransportExit; }

        bool unknown0x1          : 1;
        bool unknown0x2          : 1;
        bool unknown0x4          : 1;
        bool unknown0x8          : 1;
        bool fallingSlow         : 1;
        bool done                : 1;
        bool falling             : 1;
        bool no_spline           : 1;
        bool unknown1            : 1;
        bool flying              : 1;
        bool orientationFixed    : 1;
        bool catmullrom          : 1;
        bool cyclic              : 1;
        bool enter_cycle         : 1;
        bool frozen              : 1;
        bool transportEnter      : 1;
        bool transportExit       : 1;
        bool unknown2            : 1;
        bool unknown3            : 1;
        bool backward            : 1;
        bool smoothGroundPath    : 1;
        bool canSwim             : 1;
        bool uncompressedPath    : 1;
        bool unknown4            : 1;
        bool unknown5            : 1;
        bool animation           : 1;
        bool parabolic           : 1;
        bool fadeObject          : 1;
        bool steering            : 1;
        bool unknown8            : 1;
        bool unknown9            : 1;
        bool unknown10           : 1;
    };
#pragma pack(pop)
}

#endif // TRINITYSERVER_MOVESPLINEFLAG_H
