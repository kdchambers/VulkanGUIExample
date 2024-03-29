#ifndef TEXT_VULKAN_H
#define TEXT_VULKAN_H

#include <glm/glm.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <vector>
#include <ctype.h>
#include <string>
#include <unordered_map>
#include <tuple>

#include "typesvulkan.h"
#include "config.h"

struct GenerateTextMeshesParams
{
    uint16_t * indicesStart;
    glm::vec2 * verticesStart;
    uint16_t verticesStrideBytes;
    uint16_t startingVertexIndex;
    FontBitmap& fontBitmap;     // TODO: Refactor this out
    glm::vec2 * textureMapStart;
    uint16_t textureMapStrideBytes;
    const char * text;
    uint16_t xPos;
    uint16_t yPos;
    uint16_t boxWidth;
    uint16_t windowWidth;
    uint16_t windowHeight;
};

// TODO: Don't hardcode this stuff
const uint16_t MAX_LINE_WIDTH = 450;
const uint16_t TEXT_SPACING = 0;
const uint16_t TEXT_LINE_SPACING = 55;

inline double signedNormalizedPixelDistance(uint32_t posRatio1, uint32_t posRatio2, uint32_t globalRangePixels);

inline double signedNormalizePixelPosition(uint32_t position_pixels, uint32_t length_pixels);
inline double unsignedNormalizePixelPosition(uint32_t position_pixels, uint32_t length_pixels);

void updateMultVertexPositions( glm::vec2 * vertices,
                                uint32_t numberVertices,
                                uint32_t verticesStrideBytes,
                                float multByX,
                                float multByY );

void updateAddVertexPositions(  glm::vec2 * vertices,
                                uint32_t numberVertices,
                                uint32_t verticesStrideBytes,
                                float addToX,
                                float addToY );

bool setupBitmapForCharacterSet(    FontBitmap& font_bitmap,
                                    uint16_t texture_width_cells,
                                    uint16_t cell_size,
                                    uint16_t num_charactors);

void generateTextMeshes(GenerateTextMeshesParams& params);

void generateTextMeshes(    uint16_t * indices,
                            glm::vec2 * vertices,
                            uint16_t vertices_stride_bytes,
                            uint16_t start_vertex_index,
                            FontBitmap& font_bitmap,
                            glm::vec2 * texture_map_start,
                            uint16_t texture_map_stride,
                            std::string& text,
                            uint16_t start_x,
                            uint16_t start_y);

bool instanciateFontBitmap(FontBitmap& outFontBitmap, FT_Face& face, const char * uniqueCharsString, uint16_t textureWidthCells, uint16_t cellSize );

#endif // TEXT_H
