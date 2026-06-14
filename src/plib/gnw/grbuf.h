#ifndef FALLOUT_PLIB_GNW_GRBUF_H_
#define FALLOUT_PLIB_GNW_GRBUF_H_

void draw_line(unsigned char* buf, int pitch, int left, int top, int right, int bottom, int color);
void draw_box(unsigned char* buf, int pitch, int left, int top, int right, int bottom, int color);
void draw_shaded_box(unsigned char* buf, int pitch, int left, int top, int right, int bottom, int ltColor, int rbColor);
void cscale(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destWidth, int destHeight, int destPitch);
void trans_cscale(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destWidth, int destHeight, int destPitch);
void buf_to_buf(unsigned char* src, int width, int height, int srcPitch, unsigned char* dest, int destPitch);
void trans_buf_to_buf(unsigned char* src, int width, int height, int srcPitch, unsigned char* dest, int destPitch);
void buf_fill(unsigned char* buf, int width, int height, int pitch, int color);
void buf_texture(unsigned char* buf, int width, int height, int pitch, void* a5, int a6, int a7);
void lighten_buf(unsigned char* buf, int width, int height, int pitch);
void swap_color_buf(unsigned char* buf, int width, int height, int pitch, int color1, int color2);
void buf_outline(unsigned char* buf, int width, int height, int pitch, int color);

void cscale_8_to_32(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destWidth, int destHeight, int destPitch, unsigned char* pal);
void trans_cscale_8_to_32(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destWidth, int destHeight, int destPitch, unsigned char* pal);

#endif /* FALLOUT_PLIB_GNW_GRBUF_H_ */
