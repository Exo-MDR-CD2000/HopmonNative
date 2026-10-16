// Implementations of the single-file libraries used by the mod framework (see THIRD_PARTY_NOTICES.md).
// They are compiled here once; other files include the headers only.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_PSD
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_NO_GIF
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif
#include "../../third_party/stb_image.h"
#define CGLTF_IMPLEMENTATION
#include "../../third_party/cgltf.h"
// stb_vorbis is a .c file meant to be included once; its header part is exposed with STB_VORBIS_HEADER_ONLY.
#include "../../third_party/stb_vorbis.c"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
