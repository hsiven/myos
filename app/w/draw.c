#include "draw.h"
#include "bmp.h"
#include <libc/libc.h>
// from pyos, 大部分已经重写

static struct myos_graph_info info;
static char asctab[16 * 256];

int
draw_init()
{
	int fd;
	get_graph_info(&info);
	
	if(info.depth != 16)
	{
		printf("depth!=16\n");
		return 1;
	}
	fd = open("/share/asc16", 0);
	if(fd < 0)
	{
		printf("can't find asc16!\n");
		return 1;
	}
	read(fd, asctab, 16*256);
	close(fd);
	tty_switch(4);
	return 0;
}

int
draw_set_canvas(DrawCanvas *pcanv, u16 *new_addr)
{
	if(new_addr)
		pcanv->video = new_addr;
	else
		pcanv->video = info.base_addr;
	pcanv->x = info.x;
	pcanv->y = info.y;
	pcanv->depth = info.depth;
	return 0;
}

// 画点函数
void
draw_set_point(DrawCanvas *pcanv, u32 x, u32 y, u16 color)
{
	if(x >= pcanv->x)return;
	if(y >= pcanv->y)return;
	pcanv->video[y*(pcanv->x)+x] = color;
}

// 取得指定点色彩
u16
draw_get_point(DrawCanvas *pcanv, u32 x,u32 y)
{
	if(x >= pcanv->x)return 0;
	if(y >= pcanv->y)return 0;
	return pcanv->video[y*(pcanv->x)+x];
}

// 矩形填充函数
void
draw_fill_rect(DrawCanvas *pcanv, u32 x1, u32 y1, u32 x2, u32 y2, u16 color)
{
	u32 y;
	u16 *pos;
	u32 w;
	if(x1 >= pcanv->x) return;
	if(y1 >= pcanv->y) return;
	if(x2 >= pcanv->x) return;
	if(y2 >= pcanv->y) return;
	if(x1 > x2 || y1 > y2) return;
	
	pos = pcanv->video + (y1 * pcanv->x) + x1;
	w = x2 - x1;
#ifdef ASM_OPT
	asm ("cld");
	for(y = y1; y <= y2; y++)
	{
		asm (
		"rep;stosw"
		::"c"(w),"a"(color),"D"(pos)
		:"memory"
		);
		pos += pcanv->x;
	}
#else
	int x;
	for(y = y1; y <= y2; y++)
	{
		for(x = 0; x < w; x++)
			pos[x] = color;
		pos += pcanv->x;
	}
#endif	
}

// 清屏函数
void
draw_clear_screen(DrawCanvas *pcanv, u16 color)
{
#ifdef ASM_OPT
	asm (
	"cld\n\t"
	"rep;stosl"
	::
	"c"((pcanv->x * pcanv->y) >> 1),
	"a"((color << 16) | color),
	"D"(pcanv->video)
	:"memory"
		);
#else
	int i;
	for(i = 0; i < pcanv->x * pcanv->y; i++)
	{
		pcanv->video[i] = color;
	}
#endif
}

// 拷贝函数

//draw_copy语义：src的(x,y,w,h)矩形区域拷贝到obj对应位置
void
draw_copy(DrawCanvas *psrc, DrawCanvas *pobj, u32 x, u32 y, u32 w, u32 h)
{
	int j;
	u32 offset1;
	u32 offset2;
	
	if(x >= psrc->x) return;
	if(y >= psrc->y) return;
	if(x >= pobj->x) return;
	if(y >= pobj->y) return;
	if(x+w >= psrc->x)
		w = psrc->x - x;
	if(x+w >= pobj->x)
		w = pobj->x - x;
	if(y+h >= psrc->y)
		h = psrc->y - y;
	if(y+h >= pobj->y)
		h = pobj->y - y;

	offset1 = y*psrc->x + x;
	offset2 = y*pobj->x + x;
#ifdef ASM_OPT	
	asm ("cld");
	for(j = 0; j < h; j++)
	{
		asm (
		"rep;movsw"
		:
		:"c"(w),
		 "S"(psrc->video + offset1),
		 "D"(pobj->video + offset2)
		:"memory"
		);
		offset1 += psrc->x;
		offset2 += pobj->x;
	}
#else
	int i;
	for(j = 0; j < h; j++)
	{
		for(i = 0; i < w; i++)
		{
			pobj->video[offset2 + i] =
				psrc->video[offset1 + i];
		}

		offset1 += psrc->x;
		offset2 += pobj->x;
	}
#endif
}

//draw_windo语义：有效大小为w*h的src拷贝到obj的(x,y)坐标处
void
draw_window(DrawCanvas *psrc, DrawCanvas *pobj, u32 x, u32 y, u32 w, u32 h)
{
	int j;
	u32 offset1;
	u32 offset2;
	
	if(x >= pobj->x) return;
	if(y >= pobj->y) return;
	if(w >= psrc->x)
		w = psrc->x;
	if(h >= psrc->y)
		h = psrc->y;
	if(x+w >= pobj->x)
		w = pobj->x - x;
	if(y+h >= psrc->y)
		h = psrc->y - y;
	if(y+h >= pobj->y)
		h = pobj->y - y;

	offset1 = 0;
	offset2 = y*pobj->x + x;

#ifdef ASM_OPT
	asm ("cld");
	for(j = 0; j < h; j++)
	{
		asm (
		"rep;movsw"
		:
		:"c"(w),
		 "S"(psrc->video + offset1),
		 "D"(pobj->video + offset2)
		 //注意与draw_copy的不同之处
		:"memory"
		);
		offset1 += psrc->x;
		offset2 += pobj->x;
	}
#else
	int i;
	for(j = 0; j < h; j++)
	{
		for(i = 0; i < w; i++)
		{
			pobj->video[offset2 + i] =
				psrc->video[offset1 + i];
		}

		offset1 += psrc->x;
		offset2 += pobj->x;
	}
#endif		
}

//draw_cursor_mask
void
draw_cursor_mask(DrawCanvas *psrc, DrawCanvas *pobj, u32 x, u32 y, u16 mask)
{	
	int i, j;
	int w, h;
	u16 *psv, *pov;

	if(x >= pobj->x) return;
	if(y >= pobj->y) return;

	psv = psrc->video;
	pov = pobj->video;
	w = psrc->x;
	h = psrc->y;
	if(x+w >= pobj->x)
		w = pobj->x - x;
	if(y+h >= pobj->y)
		h = pobj->y - y;
		
	pov += y*pobj->x + x;
	
	for(j = 0; j < h; j++)
	{
		for(i = 0; i < w; i++)
		{
			if(psv[i] == mask) continue;
			pov[i] = psv[i];
		}
		psv += psrc->x;
		pov += pobj->x;
	}
}

// 画横线函数
void
draw_x_line(DrawCanvas *pdc, u32 y, u32 x1, u32 x2, u16 color)
{
#ifndef ASM_OPT
	int x;
	if(x1 > x2)return;
	for(x = x1; x <= x2; x++)
		draw_set_point(pdc, x, y, color);
#else
	if(x1 > x2)return;
	asm("cld\n\t"
	"rep;stosw"
	::"c"(x2 - x1), "D"(pdc->video + y*pdc->x + x1), "a"(color)
	:"memory"
	);
#endif
}

// 画竖线函数
void
draw_y_line(DrawCanvas *pdc, u32 x, u32 y1, u32  y2, u16 color)
{
	int y;
	for(y = y1; y <= y2; y++)
		draw_set_point(pdc, x, y, color);
}

// 画矩形函数
void
draw_rect(DrawCanvas *pdc, u32 x1, u32 y1, u32 x2, u32 y2, u16 color, int fill)
{
	draw_x_line(pdc, y1, x1, x2, color);
	draw_x_line(pdc, y2, x1, x2, color);
	draw_y_line(pdc, x1, y1, y2, color);
	draw_y_line(pdc, x2, y1, y2, color);
	if(fill)
		draw_fill_rect(pdc, x1, y1, x2, y2, color);
}

int
draw_string(DrawCanvas *pdc, u32 x, u32 y, char *text, u16 color)
{
	while(*text)
	{
		draw_ascii(pdc, x, y, *text, color);
		x += 8;
		text++;
	}
	return x;
}

void
draw_ascii(DrawCanvas *pdc, u32 x, u32 y, u32 ascii, u16 color)
{
	char *c, ch;
	int i,j;
	c = asctab + (ascii * 16);
	for( j = 0; j < 16; j++)
	{
		ch = *c++;
		
		for( i = 0; i < 8; i++)
		{
			if(ch & 1)
				pdc->video[(y + j) * pdc->x + x + (8 - i)]
					= color;
			else
				pdc->video[(y + j) * pdc->x + x + (8 - i)]
					= 0xffff;
			ch >>= 1;
		}
	}
}


// 显示汉字
/*void vesa_print_chinese( unsigned int x , unsigned int y , unsigned int pos_in_font , unsigned short color )
{
  unsigned char *chinese_font_addr = ( unsigned char * )0x30000 + pos_in_font * 32 ; // 一个汉字 32 字节

  union{
    unsigned char ch ;
    struct{
      unsigned char ch0 : 1 ;
      unsigned char ch1 : 1 ;
      unsigned char ch2 : 1 ;
      unsigned char ch3 : 1 ;
      unsigned char ch4 : 1 ;
      unsigned char ch5 : 1 ;
      unsigned char ch6 : 1 ;
      unsigned char ch7 : 1 ;
    } ;
  } u ;

  for( int j = 0 ; j < 16 ; ++j ){ // 一个汉字 16 行
    for( int i = 0 ; i < 2 ; ++i ){ // 每行 2 个字节 ( 16 位 )
      u.ch = chinese_font_addr[ j * 2 + i ] ;
      int offset = x + i * 8 ; // 一个字节 8 个像素
      if( u.ch7 ){
        vesa_draw_point( offset , y , color ) ;
      }
      if( u.ch6 ){
        vesa_draw_point( offset + 1  , y , color ) ;
      }
      if( u.ch5 ){
        vesa_draw_point( offset + 2 , y , color ) ;
      }
      if( u.ch4 ){
        vesa_draw_point( offset + 3 , y , color ) ;
      }
      if( u.ch3 ){
        vesa_draw_point( offset + 4 , y , color ) ;
      }
      if( u.ch2 ){
        vesa_draw_point( offset + 5 , y , color ) ;
      }
      if( u.ch1 ){
        vesa_draw_point( offset + 6 , y , color ) ;
      }
      if( u.ch0 ){
        vesa_draw_point( offset + 7 , y , color ) ;
      }    
    }
    y++ ; // 显示下一行
  }
}*/

// 显示 pbmp 16 bit 格式的图片
void
draw_bmp16(DrawCanvas *pdc, u32 x, u32 y, void *pbmp, u16 mask, int use_mask)
{
	// 这里只支持 windows 标准 16 位 bmp 格式图片，（1:5:5:5）
	struct bmp_bmp_head_struct *bmp_head = pbmp;
	int w = bmp_head->info_head.width;
	int h = bmp_head->info_head.height;
	int i, j;
	u16 c565;
	
	// 下面记算存储每个点的色彩的信息所在的位置
	u16 *color = (u16 *)( (u32)pbmp + bmp_head->offset );
	
	// 由于一行的字节数比须是 4 的倍数，因此，这里先计算每行需要的填充数，除 2 是因为每个像素两个字节
	int fill_length = w * 2 % 4 / 2 ; 
	
	// bmp 的存放顺序是从下到上，从左到右
	for(i = h - 1; i >= 0; --i)
	{
		for(j = 0; j < w; j++)
		{
			c565 = draw_color_555_to_565(*color);
			
			if( !use_mask || c565 != mask)
				draw_set_point(pdc, x + j, y + i, c565);
			color++;
		}
		color += fill_length;
	}
}

void
draw_bmp(DrawCanvas *pdc, u32 x, u32 y, void *pbmp, u16 mask, int use_mask)
{
	struct bmp_bmp_head_struct *bmp_head = pbmp;
	if(bmp_head->info_head.biBitCount==24)
	{
		//printf("use24\n");
		draw_bmp24(pdc, x, y, pbmp, mask, use_mask);
	}
	else
		draw_bmp16(pdc, x, y, pbmp, mask, use_mask);
}

// 显示 pbmp 24 bit 格式的图片
void
draw_bmp24(DrawCanvas *pdc, u32 x, u32 y, void *pbmp, u16 mask, int use_mask)
{
	// 这里只支持 windows 标准 24 位 bmp 格式图片，（8:8:8）
	struct bmp_bmp_head_struct *bmp_head = pbmp;
	int w = bmp_head->info_head.width;
	int h = bmp_head->info_head.height;
	int i, j;
	struct color24
	{
		u8 b, g, r;
	} *color;
	u16 c565;
	
	// 下面记算存储每个点的色彩的信息所在的位置
	color = (void *)( (u32)pbmp + bmp_head->offset );
	
	// 由于一行的字节数比须是 4 的倍数，因此，这里先计算每行需要的填充数(char)
	int fill_length = w * 3 % 4 ; 
	
	// bmp 的存放顺序是从下到上，从左到右
	for(i = h - 1; i >= 0; --i)
	{
		for(j = 0; j < w; j++)
		{
			c565 = RGB(color->r>>3,color->g>>2,color->b>>3);
			
			if( !use_mask || c565 != mask)
				draw_set_point(pdc, x + j, y + i, c565);
			color++;
		}
		color = (void *)((u32)color + fill_length);
	}
}

