/*
 * This file is part of PRO CFW.

 * PRO CFW is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * PRO CFW is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PRO CFW. If not, see <http://www.gnu.org/licenses/ .
 */

/*
	PSP VSH 24bpp text bliter
*/
#include "blit.h"

#include <pspdisplay.h>

#include "common.h"

#include "vsh.h"
#include "fonts.h"


blit_Gfx gfx = {
	.vram32 = NULL,
	.fg_color = 0x00ffffff,
	.bg_color = 0xff000000,
	.width = 0,
	.height = 0,
	.bufferwidth = 0,
	.pixelformat = 0
};


static u32 adjust_alpha(u32 col) {
	u32 c1, c2;
	u32 alpha = col >> 24;
	u8 mul;

	if (alpha == 0)	
		return col;
	if (alpha == 0xff) 
		return col;

	c1 = col & 0x00ff00ff;
	c2 = col & 0x0000ff00;
	mul = (u8)(255 - alpha);
	c1 = ((c1*mul) >> 8) & 0x00ff00ff;
	c2 = ((c2*mul) >> 8) & 0x0000ff00;
	return (alpha << 24) | c1 | c2;
}


blit_Gfx* blit_gfx_pointer(void) {
	return (blit_Gfx*)&gfx;
}


int blit_setup(void) {
	int unk;
	sceDisplayGetMode(&unk, &gfx.width, &gfx.height);
	sceDisplayGetFrameBuf((void*)&gfx.vram32, &gfx.bufferwidth, &gfx.pixelformat, PSP_DISPLAY_SETBUF_NEXTFRAME);
	if ((gfx.bufferwidth == 0) || (gfx.pixelformat != 3)) 
		return -1;

	gfx.fg_color = 0x00ffffff;
	gfx.bg_color = 0xff000000;
	return 0;
}


void blit_set_color(int fg_col,int bg_col) {
	gfx.fg_color = fg_col;
	gfx.bg_color = bg_col;
}


int blit_string(int sx,int sy,const char *msg) {
	int x,y,p;
	int offset;
	u8 code, data;
	u32 fg_col,bg_col;

	u32 col,c1,c2;
	u32 alpha;

	fg_col = adjust_alpha(gfx.fg_color);
	bg_col = adjust_alpha(gfx.bg_color);
	
	font_Data *font = (font_Data*)font_data_pointer();


//Kprintf("MODE %d WIDTH %d\n",pixelformat,bufferwidth);
	if ((gfx.bufferwidth == 0) || (gfx.pixelformat != 3))
		return -1;

	for (x = 0; msg[x] && x < (gfx.width / font->width); x++) {
		code = (u8)msg[x]; // no truncate now

		for (y = 0; y < font->height; y++) {
			offset = (sy + y) * gfx.bufferwidth + sx + x * font->width;
			data = y >= 7 ? 0x00 : font->bitmap[code * font->width + y];
			for (p = 0; p < font->height; p++) {
				col = (data & 0x80) ? fg_col : bg_col;
				alpha = col >> 24;
				if (alpha == 0) 
					gfx.vram32[offset] = col;
				else if (alpha != 0xff) {
					c2 = gfx.vram32[offset];
					c1 = c2 & 0x00ff00ff;
					c2 = c2 & 0x0000ff00;
					c1 = ((c1 * alpha) >> 8) & 0x00ff00ff;
					c2 = ((c2 * alpha) >> 8) & 0x0000ff00;
					gfx.vram32[offset] = (col & 0xffffff) + c1 + c2;
				}

				data <<= 1;
				offset++;
			}
		}
	}
	return sx + x * font->width;
}

int blit_string_ctr(int sy,const char *msg) {
	return blit_string((gfx.width - scePaf_strlen(msg) * font_data_pointer()->width) / 2, sy, msg);
}


void blit_rect_fill(int sx, int sy, int w, int h) {
	int x, y;
	u32 col, c1, c2;
	u32 bg_col;
	u32 offset, alpha;
	bg_col = adjust_alpha(gfx.bg_color);
	
	for (y = 0; y < h; y++){
		for (x = 0; x < w; x++){
			col = bg_col;
			alpha = col >> 24;
			offset = (sy + y) * gfx.bufferwidth + (sx + x);
			if(alpha == 0)
				gfx.vram32[offset] = col;
			else if (alpha != 0xff) {
				c2 = gfx.vram32[offset];
				c1 = c2 & 0x00ff00ff;
				c2 = c2 & 0x0000ff00;
				c1 = ((c1 * alpha) >> 8) & 0x00ff00ff;
				c2 = ((c2 * alpha) >> 8) & 0x0000ff00;
				gfx.vram32[offset] = (col & 0xffffff) + c1 + c2;
			}
		}
	}
}

// Returns size of string in pixels
int blit_get_string_width(char *msg) {
	return scePaf_strlen(msg) * font_data_pointer()->width;
}