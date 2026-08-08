#pragma once

bool initDestTextures();
void cleanupSrcTextures();
void cleanupDestTextures();
bool initSourceTextures();

bool updatePalette();
bool fullTileUpdate();
bool fullChunkUpdate();
bool updateMappings();
bool partialTileUpdate();