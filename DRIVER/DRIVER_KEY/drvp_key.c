#include "drvp_key.h"
#include <string.h>

#define FIFO_MAX_NUM_SIZE       256

/* ---- 状态机常量 (10ms 单位) ---- */
#define DEBOUNCE_CNT            3       /* 30ms 消抖 */
#define LONG_PRESS_CNT          100     /* 1000ms 长按 */
#define DOUBLE_CLICK_CNT        30      /* 300ms 双击等待 */

/* ---- 按键状态机枚举 ---- */
typedef enum
{
    KEY_ST_IDLE,                /* 等待按下 */
    KEY_ST_PRESS,               /* 刚按下(单次过渡) */
    KEY_ST_HOLD,                /* 按住不放,计时长按 */
    KEY_ST_LONG_PRESS,          /* 已触发长按 */
    KEY_ST_SINGLE_WAIT,         /* 松开后等待第二次点击 */
    KEY_ST_DOUBLE_PRESS,        /* 第二次按下,等待松开 */
} KEY_STATE_T;

/* ---- 按键独立状态 ---- */
typedef struct
{
    uint8_t  state;             /* KEY_ST_* */
    uint8_t  raw;               /* 原始IO电平 (1=按下,0=松开) */
    uint8_t  released;          /* 1=已松开 */
    uint8_t  debounced;         /* 消抖后的稳定值 (1=按下,0=松开) */
    uint8_t  debounce_cnt;      /* 消抖计数 */
    uint16_t hold_timer;        /* 长按计时 */
    uint16_t click_timer;       /* 双击计时 */
} key_sm_t;

typedef struct
{
    void(*init)(void);
    uint32_t(*readmask)(void);
} drv_key_type;

typedef struct
{
    circular_buffer fifo;
    key_sm_t        key_sm[E_KEY_COUNT];
    uint32_t        keymask;
} drvp_key_type;

const static drv_key_type drv_key=
{
    .init=drv_key_init,
    .readmask=drv_key_readmask
};

__attribute__((section (".RAM_D2")))static drvp_key_type drvp_key;

static void drvp_key_wfifo(E_KEY_TYPE key, E_EVENT_TYPE event)
{
    uint16_t keyevent = (uint16_t)((key << 8) | event);
    circular_buffer_write(&drvp_key.fifo, (const char*)&keyevent, 2);
}

void drvp_key_init(void)
{
    drv_key.init();
    memset(&drvp_key, 0, sizeof(drvp_key_type));
    circular_buffer_init(&drvp_key.fifo, FIFO_MAX_NUM_SIZE);
}

uint16_t drvp_key_rfifo(void)
{
    uint16_t keyevent;
    if(circular_buffer_read(&drvp_key.fifo, (char*)&keyevent, 2)) return keyevent;
    return KEY_EVENT_ERROR;
}

void drvp_key_prc_10ms(void)
{
    static key_sm_t  *ks = NULL;

    drvp_key.keymask = drv_key.readmask();

    for(int i = E_KEY_START; i < E_KEY_COUNT; i++)
    {
        ks      = &drvp_key.key_sm[i];
        ks->raw = (uint8_t)((drvp_key.keymask >> i) & 1);

        /* ========== (1) 消抖处理 ========== */
        if(ks->raw != ks->debounced)
        {
            ks->debounce_cnt++;
            if(ks->debounce_cnt >= DEBOUNCE_CNT)
            {
                ks->debounced    = ks->raw;
                ks->debounce_cnt = 0;
            }
        }
        else ks->debounce_cnt = 0;

        /* ========== (2) 状态机 ========== */
        ks->released = (!ks->debounced);

        switch(ks->state)
        {
        case KEY_ST_IDLE:
            if(!ks->released) ks->state = KEY_ST_PRESS;
            break;

        case KEY_ST_PRESS:
            drvp_key_wfifo((E_KEY_TYPE)i, E_EVENT_PRESS);
            ks->hold_timer = 0;
            ks->state = KEY_ST_HOLD;
            break;

        case KEY_ST_HOLD:
            if(ks->released)           /* 松开(未到长按) */
            {
                drvp_key_wfifo((E_KEY_TYPE)i, E_EVENT_RELEASE);
                ks->click_timer = 0;
                ks->state = KEY_ST_SINGLE_WAIT;
            }
            else
            {
                ks->hold_timer++;
                if(ks->hold_timer >= LONG_PRESS_CNT)
                {
                    drvp_key_wfifo((E_KEY_TYPE)i, E_EVENT_LPRESS);
                    ks->state = KEY_ST_LONG_PRESS;
                }
            }
            break;

        case KEY_ST_LONG_PRESS:
            if(ks->released)
            {
                drvp_key_wfifo((E_KEY_TYPE)i, E_EVENT_LPRESS_RELEASE);
                ks->state = KEY_ST_IDLE;
            }
            break;

        case KEY_ST_SINGLE_WAIT:
            if(!ks->released)           /* 第二次按下(双击) */
            {
                drvp_key_wfifo((E_KEY_TYPE)i, E_EVENT_PRESS);
                ks->hold_timer = 0;
                ks->state = KEY_ST_DOUBLE_PRESS;
            }
            else
            {
                ks->click_timer++;
                if(ks->click_timer >= DOUBLE_CLICK_CNT)
                {
                    drvp_key_wfifo((E_KEY_TYPE)i, E_EVENT_CLICK);
                    ks->state = KEY_ST_IDLE;
                }
            }
            break;

        case KEY_ST_DOUBLE_PRESS:
            if(ks->released)
            {
                drvp_key_wfifo((E_KEY_TYPE)i, E_EVENT_RELEASE);
                drvp_key_wfifo((E_KEY_TYPE)i, E_EVENT_DOUBLE_CLICK);
                ks->state = KEY_ST_IDLE;
            }
            break;
        }
    }
}
