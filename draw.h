//
// Created by alexoxorn on 8/4/26.
//

#ifndef DRAW_H
#define DRAW_H
bool drawToLevel(bool prio);
bool drawToBackground(bool prio);
bool drawSprites(bool prio);
bool drawRings();
bool drawHudText();

namespace DEBUG {
    extern bool chunkInfo;
    extern bool swapFGBG;
    extern bool forceFG;
}
#endif // DRAW_H
