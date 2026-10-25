/*
 * C fallback for the NEON tiled-to-planar helpers. The assembly versions in
 * tiled_yuv.S are ARM32-only; on aarch64 the symbols would be missing.
 *
 * Sunxi tiled NV12 layout: 32x32-pixel tiles, each stored as 32 rows of 32
 * bytes. Luma tiles cover the full frame; chroma tiles are 16x16 with packed
 * U/V pairs (handled by tiled_deinterleave_to_planar).
 */

#include <stdint.h>
#include <string.h>

#include "tiled_yuv.h"

void tiled_to_planar(void *src, void *dst, unsigned int dst_pitch,
		     unsigned int width, unsigned int height)
{
	unsigned int tiles_x = (width + 31) / 32;
	unsigned int tiles_y = (height + 31) / 32;
	unsigned int tile_x, tile_y, y, x;

	for (tile_y = 0; tile_y < tiles_y; tile_y++) {
		for (tile_x = 0; tile_x < tiles_x; tile_x++) {
			uint8_t *tile = (uint8_t *)src +
				(tile_y * tiles_x + tile_x) * 1024;
			unsigned int rows = 32;
			unsigned int cols = 32;

			if ((tile_y + 1) * 32 > height)
				rows = height - tile_y * 32;
			if ((tile_x + 1) * 32 > width)
				cols = width - tile_x * 32;

			for (y = 0; y < rows; y++) {
				memcpy((uint8_t *)dst + (tile_y * 32 + y) *
				       dst_pitch + tile_x * 32,
				       tile + y * 32, cols * sizeof(uint8_t));
			}
		}
	}
}

void tiled_deinterleave_to_planar(void *src, void *dst1, void *dst2,
				  unsigned int dst_pitch, unsigned int width,
				  unsigned int height)
{
	unsigned int tiles_x = (width + 31) / 32;
	unsigned int tiles_y = (height + 15) / 16;
	unsigned int tile_x, tile_y, y, x;

	for (tile_y = 0; tile_y < tiles_y; tile_y++) {
		for (tile_x = 0; tile_x < tiles_x; tile_x++) {
			uint8_t *tile = (uint8_t *)src +
				(tile_y * tiles_x + tile_x) * 1024;
			unsigned int rows = 16;
			unsigned int cols = 32;

			if ((tile_y + 1) * 16 > height)
				rows = height - tile_y * 16;
			if ((tile_x + 1) * 32 > width)
				cols = width - tile_x * 32;

			for (y = 0; y < rows; y++) {
				for (x = 0; x < cols; x += 2) {
					uint8_t u = tile[y * 32 + x];
					uint8_t v = tile[y * 32 + x + 1];

					((uint8_t *)dst1)[(tile_y * 16 + y) *
						dst_pitch + tile_x * 16 + x / 2] = u;
					((uint8_t *)dst2)[(tile_y * 16 + y) *
						dst_pitch + tile_x * 16 + x / 2] = v;
				}
			}
		}
	}
}
