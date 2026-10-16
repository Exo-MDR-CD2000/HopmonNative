// Replacement for the DX7 SDK texture manager (d3dtextr.cpp) and DirectDraw helpers (ddutil.cpp).
#pragma once
#include "../core/d3dtypes.h"

class Texture;
class Surface;

// Note the argument order: Hopmon's variant takes the flags before the stage (the DX7 SDK's
// CreateTextureFromFile is name, stage, flags). Confirmed by the PDB parameter names.
// flags: D3DTEXTR_TRANSPARENTWHITE (1) / D3DTEXTR_TRANSPARENTBLACK (2) make pure white / black
// pixels transparent. Hopmon uses TRANSPARENTBLACK for the steam puff (0xdc).
HRESULT D3DTextr_CreateTextureFromResource(WORD resId, DWORD flags, DWORD stage);
Texture* D3DTextr_GetSurface(WORD resId);
HRESULT D3DTextr_DestroyTexture(WORD resId);
void D3DTextr_InvalidateAllTextures();
void D3DTextr_InvalidateTexture(WORD resId);  // rebuild one texture on next use (settings changed)
void D3DTextr_RestoreAllTextures();
void D3DTextr_FreeAll();
// Native (mods): re-read every texture's image (a mod may now replace it) and drop textures of
// mod-only images (their ids are handed out afresh when mods are re-read).
void D3DTextr_ReloadAll();

// ddutil.cpp
Surface* DDLoadBitmap(WORD resId, int* width, int* height);
HRESULT DDSetColorKey(Surface* s, COLORREF rgb);
