/**
 * @file lv_port_indev_templ.c
 *
 */

/*Copy this file as "lv_port_indev.c" and set this value to "1" to enable content*/
#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_indev.h"
#include "lvgl.h"

/* Touch panel, LCD, key, and UART peripheral headers */
#include "./BSP/TOUCH/touch.h"
#include "./BSP/LCD/lcd.h"
#include "./BSP/KEY/key.h"
#include "./SYSTEM/usart/usart.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
/* Touchpad */
static void touchpad_init(void);
static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);
static bool touchpad_is_pressed(void);
static void touchpad_get_xy(lv_coord_t * x, lv_coord_t * y);

/* Mouse (optional) */
//static void mouse_init(void);
//static void mouse_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);
//static bool mouse_is_pressed(void);
//static void mouse_get_xy(lv_coord_t * x, lv_coord_t * y);

/* Keypad (optional) */
//static void keypad_init(void);
//static void keypad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);
//static uint32_t keypad_get_key(void);

/* Encoder (optional) */
//static void encoder_init(void);
//static void encoder_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);
//static void encoder_handler(void);

/* Buttons (optional) */
//static void button_init(void);
//static void button_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);
//static int8_t button_get_pressed_id(void);
//static bool button_is_pressed(uint8_t id);

/**********************
 *  STATIC VARIABLES
 **********************/
lv_indev_t * indev_touchpad;	// Touchpad input device handle
//lv_indev_t * indev_mouse;			// Mouse input device handle
//lv_indev_t * indev_keypad;		// Keypad input device handle
//lv_indev_t * indev_encoder;		// Encoder input device handle
//lv_indev_t * indev_button;		// Buttons input device handle

/* Encoder variables (optional) */
//static int32_t encoder_diff;
//static lv_indev_state_t encoder_state;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief       Initialize all enabled input devices for LVGL
 * @param       None
 * @retval      None
 */
void lv_port_indev_init(void)
{
    /**
     * LVGL supports the following input device types:
     *  - Touchpad (pointer input)
     *  - Mouse (pointer input, with cursor support)
     *  - Keypad (for navigating GUI elements)
     *  - Encoder (for navigating GUI elements with rotation: up/down/select)
     *  - Buttons (screen-mounted physical buttons)
     *
     * Only enable and implement the `..._read()` callback for the devices you need.
     * Initialize the corresponding hardware peripherals before registering the device.
     */

    static lv_indev_drv_t indev_drv;

    /*------------------
     * Touchpad
     * -----------------*/

    /* Initialize touchpad hardware (implement in touchpad_init()) */
    touchpad_init();

    /* Register touchpad as a pointer input device */
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    indev_touchpad = lv_indev_drv_register(&indev_drv);

    /*------------------
     * Mouse (optional)
     * -----------------*/

    /* Initialize mouse hardware (implement in mouse_init()) */
//    mouse_init();

    /* Register mouse as a pointer input device */
//    lv_indev_drv_init(&indev_drv);
//    indev_drv.type = LV_INDEV_TYPE_POINTER;
//    indev_drv.read_cb = mouse_read;
//    indev_mouse = lv_indev_drv_register(&indev_drv);

    /* Create a cursor for the mouse (optional) */
//    lv_obj_t * mouse_cursor = lv_img_create(lv_scr_act());
//    lv_img_set_src(mouse_cursor, LV_SYMBOL_HOME);
//    lv_indev_set_cursor(indev_mouse, mouse_cursor);

    /*------------------
     * Keypad (optional)
     * -----------------*/
		 
    /* Initialize keypad hardware (implement in keypad_init()) */
//    keypad_init();

    /* Register keypad as a keypad input device */
//    lv_indev_drv_init(&indev_drv);
//    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
//    indev_drv.read_cb = keypad_read;
//    indev_keypad = lv_indev_drv_register(&indev_drv);

//    /* To use the keypad, create a group with `lv_group_t * group = lv_group_create()`
//     * Add GUI objects to the group with `lv_group_add_obj(group, obj)`
//     * Associate the keypad with the group using:
//     * `lv_indev_set_group(indev_keypad, group);` */

    /*------------------
     * Encoder (optional)
     * -----------------*/

    /* Initialize encoder hardware (implement in encoder_init()) */
//    encoder_init();

    /* Register encoder as an encoder input device */
//    lv_indev_drv_init(&indev_drv);
//    indev_drv.type = LV_INDEV_TYPE_ENCODER;
//    indev_drv.read_cb = encoder_read;
//    indev_encoder = lv_indev_drv_register(&indev_drv);

//    /* To use the encoder, create a group with `lv_group_t * group = lv_group_create()`
//     * Add GUI objects to the group with `lv_group_add_obj(group, obj)`
//     * Associate the encoder with the group using:
//     * `lv_indev_set_group(indev_encoder, group);` */

    /*------------------
     * Buttons (optional)
     * -----------------*/

//    /* Initialize buttons hardware (implement in button_init()) */
//    button_init();

//    /* Register buttons as a button input device */
//    lv_indev_drv_init(&indev_drv);
//    indev_drv.type = LV_INDEV_TYPE_BUTTON;
//    indev_drv.read_cb = button_read;
//    indev_button = lv_indev_drv_register(&indev_drv);

//    /* Define screen positions for each button (maps physical buttons to screen areas)
//     * The points specify the "hotspot" of each button on the screen */
//    static const lv_point_t btn_points[2] = {
//            {10, 10},   /*Button 0 -> x:10; y:10*/
//            {40, 100},  /*Button 1 -> x:40; y:100*/
//    };
//    lv_indev_set_button_points(indev_button, btn_points);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*------------------
 * Touchpad
 * -----------------*/

/**
 * @brief       Initialize touchpad hardware and calibration
 * @param       None
 * @retval      None
 */
static void touchpad_init(void)
{
    /* Initialize touchpad controller */
    tp_dev.init();
    printf("touch_Init\r\n"); /* Print touchpad init status */

    /* Perform touchpad calibration if KEY0 is pressed */
    if (key_scan(0) == KEY0_PRES)           /* KEY0 pressed: start calibration */
    {
        lcd_clear(WHITE);                   /* Clear LCD screen for calibration */
        tp_adjust();                        /* Perform touchpad calibration */
        tp_save_adjust_data();              /* Save calibration data to storage */
    }
}

/**
 * @brief       Read touchpad state and coordinates (called by LVGL)
 * @param       indev_drv   : Pointer to the input device driver descriptor
 * @param       data        : Pointer to store input device data (state + coordinates)
 * @retval      None
 */
static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    static lv_coord_t last_x = 0;
    static lv_coord_t last_y = 0;

    /* Check if touchpad is pressed */
    if(touchpad_is_pressed()) {
       // printf("LV_INDEV_STATE_PR\r\n"); /* Print pressed state */
        touchpad_get_xy(&last_x, &last_y); /* Get current coordinates */
        data->state = LV_INDEV_STATE_PR;   /* Set state to PRESSED */
    }
    else {
        //printf("LV_INDEV_STATE_REL\r\n"); /* Print released state */
        data->state = LV_INDEV_STATE_REL; /* Set state to RELEASED */
    }

    /* Maintain last coordinates even when released (required by LVGL) */
    data->point.x = last_x;
    data->point.y = last_y;
}

/**
 * @brief       Check if the touchpad is currently pressed
 * @param       None
 * @retval      true        : Touchpad is pressed
 *              false       : Touchpad is released
 */
static bool touchpad_is_pressed(void)
{
    /* Scan touchpad for press event */
    tp_dev.scan(0);

    /* Check touchpad status flag */
    if (tp_dev.sta & TP_PRES_DOWN)
    {
        //printf("touch1ok\r\n"); /* Print press detection status */
        return true;
    }
    return false;
}

/**
 * @brief       Get current touchpad coordinates when pressed
 * @param       x   : Pointer to store X coordinate
 * @param       y   : Pointer to store Y coordinate
 * @retval      None
 */
static void touchpad_get_xy(lv_coord_t * x, lv_coord_t * y)
{
    /* Read raw coordinates from touchpad controller */
    (*x) = tp_dev.x[0];
    (*y) = tp_dev.y[0];

    /* Print coordinates for debugging */
//    printf("x:%x\r\n", tp_dev.x[0]);
//    printf("Y:%x\r\n", tp_dev.y[0]);

    /* Optional: Draw test point/line (commented out) */
    //lcd_draw_point(*x, *y, RED);
    //lcd_draw_hline(*x, *y, 50, RED);
}

/*------------------
 * Mouse (optional)
 * -----------------*/

/**
 * @brief       Initialize mouse hardware (optional)
 * @param       None
 * @retval      None
 */
//static void mouse_init(void)
//{
//    /*Your code comes here*/
//    tp_dev.init();
//    /* Perform calibration if touchpad is not already calibrated */
//    if (0 == (tp_dev.touchtype & 0x80))
//    {
//        tp_adjust();
//        tp_save_adjust_data();
//    }
//}

/**
 * @brief       Read mouse state and coordinates (called by LVGL, optional)
 * @param       indev_drv   : Pointer to the input device driver descriptor
 * @param       data        : Pointer to store input device data (state + coordinates)
 * @retval      None
 */
//static void mouse_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
//{
//    /* Get current X and Y coordinates */
//    mouse_get_xy(&data->point.x, &data->point.y);

//    /* Check if mouse button is pressed */
//    if(mouse_is_pressed()) {
//        data->state = LV_INDEV_STATE_PR;
//    } else {
//        data->state = LV_INDEV_STATE_REL;
//    }
//}

/**
 * @brief       Check if the mouse button is pressed (optional)
 * @param       None
 * @retval      true        : Mouse button is pressed
 *              false       : Mouse button is released
 */
//static bool mouse_is_pressed(void)
//{
//    /*Your code comes here*/
//    tp_dev.scan(0);
//    
//    if (tp_dev.sta & TP_PRES_DOWN)
//    {
//        return true;
//    }
//    
//    return false;
//}

/**
 * @brief       Get current mouse coordinates (optional)
 * @param       x   : Pointer to store X coordinate
 * @param       y   : Pointer to store Y coordinate
 * @retval      None
 */
//static void mouse_get_xy(lv_coord_t * x, lv_coord_t * y)
//{
//    /*Your code comes here*/
//    (*x) = tp_dev.x[0];
//    (*y) = tp_dev.y[0];
//}

/*------------------
 * Keypad (optional)
 * -----------------*/

///**
// * @brief       Initialize keypad hardware (optional)
// * @param       None
// * @retval      None
// */
//static void keypad_init(void)
//{
//    /*Your code comes here*/
//}

///**
// * @brief       Read keypad state (called by LVGL, optional)
// * @param       indev_drv : Pointer to the input device driver descriptor
// * @param       data      : Pointer to store input device data (state + key code)
// * @retval      None
// */
//static void keypad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
//{
//    static uint32_t last_key = 0;

//    /* Get currently pressed key (0 = no key pressed) */
//    uint32_t act_key = keypad_get_key();
//    if(act_key != 0) {
//        data->state = LV_INDEV_STATE_PR;

//        /* Map custom key codes to LVGL standard key codes */
//        switch(act_key) {
//        case 1:
//            act_key = LV_KEY_NEXT;
//            break;
//        case 2:
//            act_key = LV_KEY_PREV;
//            break;
//        case 3:
//            act_key = LV_KEY_LEFT;
//            break;
//        case 4:
//            act_key = LV_KEY_RIGHT;
//            break;
//        case 5:
//            act_key = LV_KEY_ENTER;
//            break;
//        }

//        last_key = act_key;
//    } else {
//        data->state = LV_INDEV_STATE_REL;
//    }

//    data->key = last_key;
//}

///**
// * @brief       Get the currently pressed keypad key (optional)
// * @param       None
// * @retval      0 : No key pressed
// *              Other values: Custom key code of the pressed key
// */
//static uint32_t keypad_get_key(void)
//{
//    /*Your code comes here*/
//    return 0;
//}

/*------------------
 * Encoder (optional)
 * -----------------*/

///**
// * @brief       Initialize encoder hardware (optional)
// * @param       None
// * @retval      None
// */
//static void encoder_init(void)
//{
//    /*Your code comes here*/
//}

///**
// * @brief       Read encoder state (called by LVGL, optional)
// * @param       indev_drv : Pointer to the input device driver descriptor
// * @param       data      : Pointer to store input device data (rotation diff + state)
// * @retval      None
// */
//static void encoder_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
//{
//    data->enc_diff = encoder_diff;
//    data->state = encoder_state;
//}

///**
// * @brief       Encoder interrupt handler (optional)
// *              Call this function in the encoder's GPIO interrupt service routine
// *              to update rotation difference and press state
// * @param       None
// * @retval      None
// */
//static void encoder_handler(void)
//{
//    /*Your code comes here*/
//    encoder_diff += 0;
//    encoder_state = LV_INDEV_STATE_REL;
//}

/*------------------
 * Buttons (optional)
 * -----------------*/

///**
// * @brief       Initialize physical buttons (optional)
// * @param       None
// * @retval      None
// */
//static void button_init(void)
//{
//    /*Your code comes here*/
//}

///**
// * @brief       Read button state (called by LVGL, optional)
// * @param       indev_drv : Pointer to the input device driver descriptor
// * @param       data      : Pointer to store input device data (state + button ID)
// * @retval      None
// */
//static void button_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
//{
//    static uint8_t last_btn = 0;

//    /* Get ID of the currently pressed button (-1 = no button pressed) */
//    int8_t btn_act = button_get_pressed_id();

//    if(btn_act >= 0) {
//        data->state = LV_INDEV_STATE_PR;
//        last_btn = btn_act;
//    } else {
//        data->state = LV_INDEV_STATE_REL;
//    }

//    /* Maintain last pressed button ID even when released */
//    data->btn_id = last_btn;
//}

///**
// * @brief       Get ID of the currently pressed button (optional)
// * @param       None
// * @retval      -1 : No button pressed
// *              Other values: ID of the pressed button
// */
//static int8_t button_get_pressed_id(void)
//{
//    uint8_t i;

//    /* Check all buttons for press (adjust loop count to match your button count) */
//    for(i = 0; i < 2; i++) {
//        /* Return ID of the first pressed button */
//        if(button_is_pressed(i)) {
//            return i;
//        }
//    }

//    /* No button pressed */
//    return -1;
//}

///**
// * @brief       Check if a specific button is pressed (optional)
// * @param       id : ID of the button to check
// * @retval      true        : Button is pressed
// *              false       : Button is released
// */
//static bool button_is_pressed(uint8_t id)
//{
//    /*Your code comes here*/
//    return false;
//}

#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;

#endif

