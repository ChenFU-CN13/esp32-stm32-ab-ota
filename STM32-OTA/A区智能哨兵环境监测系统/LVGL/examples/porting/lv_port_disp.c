/**
 * @file lv_port_disp_templ.c
 *
 */

/*Copy this file as "lv_port_disp.c" and set this value to "1" to enable content*/
#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_disp.h"
#include <stdbool.h>
#include "./BSP/LCD/lcd.h"
#include "DMA_LCD.h"
#include "./BSP/SRAM/sram.h"  // 新增：引入SRAM驱动头文件
#include "./SYSTEM/usart/usart.h"
/*********************
 *      DEFINES
 *********************/
#define MY_DISP_HOR_RES    800   /* width */
#define MY_DISP_VER_RES    480   /* height */

//1. 核心配置：单个缓冲区64行（刚好100KB）
#define LVGL_BUF_ROWS      128 // 可自由修改：10/15/20/30等
//2.计算单个缓冲区字节数（宏定义，编译期计算，无运行开销）
#define LVGL_BUF_SIZE_BYTES (MY_DISP_HOR_RES * LVGL_BUF_ROWS * sizeof(lv_color_t))


//3.外部SRAM地址分配（重点：双缓冲区总大小200KB，需保证地址不重叠、不超1MB外部SRAM）
// // 缓冲区1起始地址（偏移0x1000，避开SRAM起始段）
#define LVGL_BUF1_ADDR  (SRAM_BASE_ADDR + 0x1000)
// 缓冲区2起始地址 = 缓冲区1地址 + 单个缓冲区字节数 + 0x400（1024字节）预留间隔（避免边界冲突）
#define LVGL_BUF2_ADDR  (LVGL_BUF1_ADDR + LVGL_BUF_SIZE_BYTES + 0x800)


//volatile uint8_t lv_gpu_state = 0;
/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
// static lv_disp_drv_t disp_drv;                         /*Descriptor of a display driver*/  /* ��ʾ�豸�������� */
  lv_disp_drv_t disp_drv;


static void disp_init(void);

static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p);
//static void gpu_fill(lv_disp_drv_t * disp_drv, lv_color_t * dest_buf, lv_coord_t dest_width,		/* GPU ��亯��(ʹ��GPUʱ����Ҫʵ��) */
//        const lv_area_t * fill_area, lv_color_t color);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/


/**
 * @brief       Fast fill a rectangular area with RGB color data
 * @param       sx, sy: Start coordinates (top-left corner)
 * @param       ex, ey: End coordinates (bottom-right corner)
 * @param       color: Pointer to the RGB color data array (format matches LCD color depth)
 * @retval      None
 */
void lcd_draw_fast_rgb_color(int16_t sx, int16_t sy,int16_t ex, int16_t ey, uint16_t *color)
{
    uint16_t w = ex-sx+1;
    uint16_t h = ey-sy+1;

    lcd_set_window(sx, sy, w, h);
    uint32_t draw_size = w * h;
    lcd_write_ram_prepare();

    for(uint32_t i = 0; i < draw_size; i++)
    {
        lcd_wr_data(color[i]);
    }
}

void lv_port_disp_init(void)
{
    /*-------------------------
     * Initialize your display
     * -----------------------*/
    disp_init();

    /*-----------------------------
     * Create a buffer for drawing
     *----------------------------*/

    /**
     * LVGL requires a buffer where it draws the screen content.
     * The buffer is passed to the display driver's `flush_cb` to copy the content to the physical display.
     * The buffer size should be at least the size of one display row.
     *
     * There are 3 buffer configurations:
     * 1. Single buffer:
     *      LVGL draws the screen content into the buffer and then sends it to the display.
     *      Simple but may cause flicker if the display is updated while drawing.
     *
     * 2. Double buffer:
     *      LVGL draws into one buffer while the other is being sent to the display (e.g. via DMA).
     *      Eliminates flicker as the display always shows a complete frame.
     *      Recommended for most applications.
     *
     * 3. Full-size double buffer:
     *      Two buffers with the full size of the display. Requires `disp_drv.full_refresh = 1`.
     *      LVGL will pass the entire screen content to `flush_cb` instead of partial areas.
     */

    /* Example for 1) Single buffer */
//    static lv_disp_draw_buf_t draw_buf_dsc_1;
//    static lv_color_t buf_1[MY_DISP_HOR_RES * 10];                          /*A buffer for 10 rows*/
//    lv_disp_draw_buf_init(&draw_buf_dsc_1, buf_1, NULL, MY_DISP_HOR_RES * 10);   /*Initialize the display buffer*/

	// 新代码（外部SRAM）：
//	static lv_disp_draw_buf_t draw_buf_dsc_1;
//	// 外部SRAM起始地址 + 偏移（比如偏移0x1000，避免地址冲突）
//	#define LVGL_SRAM_BUF_ADDR  (SRAM_BASE_ADDR + 0x1000)
//	// 缓冲区指针指向外部SRAM地址，大小：800*10个lv_color_t（16位色=2字节，共16000字节）
//	static lv_color_t *buf_1 = (lv_color_t *)LVGL_SRAM_BUF_ADDR;
//	// 初始化缓冲区：buf_1是外部SRAM指针，NULL表示单缓冲区，大小不变
//	lv_disp_draw_buf_init(&draw_buf_dsc_1, buf_1, NULL, MY_DISP_HOR_RES * 10);


    /* Example for 2) Double buffer (used here) */
//    static lv_disp_draw_buf_t draw_buf_dsc_2;
//    static lv_color_t buf_2_1[MY_DISP_HOR_RES * 15];                        /* First buffer: 15 rows of display resolution */
//    static lv_color_t buf_2_2[MY_DISP_HOR_RES * 15];                        /* Second buffer: 15 rows of display resolution */
//    lv_disp_draw_buf_init(&draw_buf_dsc_2, buf_2_1, buf_2_2, MY_DISP_HOR_RES * 15);    /* Initialize the display buffer descriptor */
	
	
	// 双缓冲区：两个缓冲区都放在外部SRAM，地址错开
	/*调整外部 SRAM 地址偏移（关键！避免重叠）
	修改行数后，单个缓冲区字节数会变，必须同步调整LVGL_BUF2_ADDR的偏移，确保两个缓冲区地址不重叠。
	LVGL_BUF2_ADDR = LVGL_BUF1_ADDR + 单个缓冲区字节数 + 预留间隔（建议0x100=256字节，避免边界问题）
	如果缓冲区重叠会导致花屏*/




	static lv_disp_draw_buf_t draw_buf_dsc_2;
	// 指针指向外部SRAM地址
	static lv_color_t *buf_2_1 = (lv_color_t *)LVGL_BUF1_ADDR;
	static lv_color_t *buf_2_2 = (lv_color_t *)LVGL_BUF2_ADDR;

	// 初始化双缓冲区：大小=800×20
	lv_disp_draw_buf_init(&draw_buf_dsc_2, buf_2_1, buf_2_2, MY_DISP_HOR_RES * LVGL_BUF_ROWS);

	
	
   /* Example for 3) Full-size double buffer (requires disp_drv.full_refresh = 1) */
//    static lv_disp_draw_buf_t draw_buf_dsc_3;
//    static lv_color_t buf_3_1[MY_DISP_HOR_RES * MY_DISP_VER_RES];             /* First full-size buffer */
//    static lv_color_t buf_3_2[MY_DISP_HOR_RES * MY_DISP_VER_RES];            /* Second full-size buffer */
//    lv_disp_draw_buf_init(&draw_buf_dsc_3, buf_3_1, buf_3_2,
//                          MY_DISP_VER_RES * LV_VER_RES_MAX);   /* Initialize the display buffer descriptor */

    /*-----------------------------------
     * Register the display in LVGL
     *----------------------------------*/

//    static lv_disp_drv_t disp_drv;                         /* Descriptor of a display driver */
    lv_disp_drv_init(&disp_drv);                           /* Initialize the driver with default values */

    /* Set up the functions to access your display */

    /* Set the resolution of the display */
    disp_drv.hor_res = MY_DISP_HOR_RES;
    disp_drv.ver_res = MY_DISP_VER_RES;

    /* Callback to copy the buffer content to the physical display */
    disp_drv.flush_cb = disp_flush;

    /* Assign the display buffer descriptor */
    disp_drv.draw_buf = &draw_buf_dsc_2;

    /* Required for Example 3) Full-size double buffer */
    //disp_drv.full_refresh = 1;

    /* Fill a memory array with a color if you have a GPU.
     * Note that in lv_conf.h you can enable GPUs with built-in support in LVGL.
     * For custom GPUs, implement this callback to accelerate color filling.*/
    //disp_drv.gpu_fill_cb = gpu_fill;

    /* Finally register the display driver with LVGL */
    lv_disp_drv_register(&disp_drv);

	
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * @brief       Initialize the physical display and required peripheral devices
 * @param       None
 * @retval      None
 */
static void disp_init(void)
{
    /* User-specific initialization code */
    lcd_init();         /* Initialize LCD controller */
    lcd_display_dir(1); /* Set display orientation (1 = specific orientation) */
    dma_lcd_init();     /* Initialize DMA for LCD data transfer */
}


volatile bool disp_flush_enabled = true;

/* Enable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_enable_update(void)
{
    disp_flush_enabled = true;
}

/* Disable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_disable_update(void)
{
    disp_flush_enabled = false;
}


/**
 * @brief       Flush a specific area of the display with the given color data
 * @note        This function is called by LVGL to update a portion of the screen.
 *              If using DMA or other hardware acceleration, implement the transfer here.
 *              MUST call `lv_disp_flush_ready()` after the data transfer is complete to notify LVGL.
 *
 * @param       disp_drv    : Pointer to the display driver descriptor
 * @param       area        : Pointer to the area to be flushed (coordinates and size)
 * @param       color_p     : Pointer to the color data buffer to write to the display
 *
 * @retval      None
 */
static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
    /* LVGL provides the color data for the area to be flushed. Implement the actual display update here. */
//    if(disp_flush_enabled) {
//        /*The most simple case (but also the slowest) to put all pixels to the screen one-by-one*/
//
//        int32_t x;
//        int32_t y;
//        for(y = area->y1; y <= area->y2; y++) {
//            for(x = area->x1; x <= area->x2; x++) {
//                /*Put a pixel to the display. For example:*/
//                /*put_px(x, y, *color_p)*/
//                color_p++;
//            }
//        }
//    }

    /* Alternative: Use fast RGB fill function to update the area */
    // lcd_draw_fast_rgb_color(area->x1, area->y1, area->x2, area->y2, (uint16_t*)color_p);

    /* IMPORTANT!!!
     * Notify LVGL that the flush operation is complete */
    //lv_disp_flush_ready(disp_drv);   //**

    if(!disp_flush_enabled) {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    uint16_t width = area->x2 - area->x1 + 1;
    uint16_t height = area->y2 - area->y1 + 1;

    lcd_set_window(area->x1, area->y1, width, height);  /* Set the display's active window to the flush area */
    lcd_write_ram_prepare();                             /* Prepare the LCD to accept RAM write operations */

    dma_lcd_start_transfer((uint16_t *)color_p, width * height);  /* Start DMA transfer of color data */
    // Note: Call `lv_disp_flush_ready(disp_drv)` in the DMA transfer completion interrupt!
	
	
	/*原始块CPU直接填充*/
//	lcd_color_fill(area->x1,area->y1,area->x2,area->y2,(uint16_t*)color_p);
//	        lv_disp_flush_ready(disp_drv);
}

/* Optional: GPU Interface */

/**
 * @brief       GPU-accelerated color fill function
 * @note        Implement this if your MCU has hardware graphics acceleration (GPU).
 *              LVGL will use this to speed up filling rectangular areas with a solid color.
 *
 * @param       disp_drv    : Pointer to the display driver descriptor
 * @param       dest_buf    : Pointer to the destination buffer to fill
 * @param       dest_width  : Width of the destination buffer (in pixels)
 * @param       fill_area   : Pointer to the rectangular area to fill
 * @param       color       : Solid color to use for filling
 *
 * @retval      None
 */
//static void gpu_fill(lv_disp_drv_t * disp_drv, lv_color_t * dest_buf, lv_coord_t dest_width,
//                    const lv_area_t * fill_area, lv_color_t color)
//{
//    /*It's an example code which should be done by your GPU*/
//    int32_t x, y;
//    dest_buf += dest_width * fill_area->y1; /*Go to the first line of the fill area*/
//
//    for(y = fill_area->y1; y <= fill_area->y2; y++) {
//        for(x = fill_area->x1; x <= fill_area->x2; x++) {
//            dest_buf[x] = color;
//        }
//        dest_buf += dest_width;    /*Go to the next line of the destination buffer*/
//    }
//}

#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif
