#include "ltdc_lcd_disp_jpeg.h"
#include "ff.h"

#define DISP_W          (LCD_WIN_X1 - LCD_WIN_X0)
#define DISP_H          (LCD_WIN_Y1 - LCD_WIN_Y0)
extern uint16_t LTDC_DISPLAY[(LCD_WIN_X1-LCD_WIN_X0)*(LCD_WIN_Y1-LCD_WIN_Y0)];

#define JPEG_CHUNK_IN   4096U
#define JPEG_CHUNK_OUT  3840U
#define JPEG_POS_X      100U
#define JPEG_POS_Y      50U

__attribute__((section(".RAM_D1"), aligned(4)))  static FIL                jpeg_file;

__attribute__((section(".RAM_D2"), aligned(32))) static JPEG_HandleTypeDef JPEG_Handle;
__attribute__((section(".RAM_D2"))) static JPEG_ConfTypeDef                JPEG_Info;
__attribute__((section(".RAM_D2"), aligned(32))) static uint8_t            JPEG_InBuffer[JPEG_CHUNK_IN];
__attribute__((section(".RAM_D2"), aligned(32))) static uint8_t            JPEG_OutBuffer[JPEG_CHUNK_OUT];

static uint32_t jpeg_input_len;
static uint32_t jpeg_mcu_size;
static uint32_t jpeg_mcu_w;
static uint32_t jpeg_mcu_h;
static uint32_t jpeg_mcu_cols;
static uint32_t jpeg_mcu_rows;
static uint32_t jpeg_mcu_index;
static uint32_t jpeg_draw_x;
static uint32_t jpeg_draw_y;
static volatile uint8_t jpeg_decode_done;
static volatile uint8_t jpeg_error;

static HAL_StatusTypeDef jpeg_decode_to_ltdc(const char *filename)
{
    FRESULT           fr;
    UINT              bytes_read;
    HAL_StatusTypeDef status;

    fr = f_open(&jpeg_file, filename, FA_READ);
    if (fr != FR_OK) return HAL_ERROR;

    jpeg_input_len = 0;
    jpeg_mcu_index = 0;
    jpeg_decode_done = 0;
    jpeg_error = 0;
    memset(&JPEG_Info, 0, sizeof(JPEG_Info));

    fr = f_read(&jpeg_file, JPEG_InBuffer, sizeof(JPEG_InBuffer), &bytes_read);
    if (fr != FR_OK || bytes_read == 0U) goto exit;

    jpeg_input_len = bytes_read;

    status = HAL_JPEG_Decode_IT(&JPEG_Handle,
															  JPEG_InBuffer,
															  jpeg_input_len,
															  JPEG_OutBuffer,
															  sizeof(JPEG_OutBuffer));
		while(!jpeg_decode_done && !jpeg_error);
		jpeg_decode_done=0;

    if(status!=HAL_OK || jpeg_error) status=HAL_ERROR;
        
exit:
    if(f_close(&jpeg_file)!=FR_OK)   status=HAL_ERROR;

    return status;
}

void ltdc_lcd_disp_jpeg_init(void)
{
	__HAL_RCC_JPGDECEN_CLK_ENABLE();
	memset(&JPEG_Handle, 0, sizeof(JPEG_Handle));
	memset(&JPEG_Info, 0, sizeof(JPEG_Info));
	memset(&jpeg_file,0,sizeof(FIL));
	JPEG_Handle.Instance = JPEG;
	HAL_JPEG_Init(&JPEG_Handle);
	HAL_NVIC_SetPriority(JPEG_IRQn, 0x0E, 0);
  HAL_NVIC_EnableIRQ(JPEG_IRQn); 
}

HAL_StatusTypeDef ltdc_lcd_disp_jpeg_show(const char *filename)
{
    return jpeg_decode_to_ltdc(filename);
}

void HAL_JPEG_InfoReadyCallback(JPEG_HandleTypeDef *hjpeg,
                                JPEG_ConfTypeDef *pInfo)
{
    JPEG_Info = *pInfo;

    switch (JPEG_Info.ChromaSubsampling) 
	 {
    case JPEG_420_SUBSAMPLING:
        jpeg_mcu_w = 16U;
        jpeg_mcu_h = 16U;
        jpeg_mcu_size = 384U;
        break;

    case JPEG_422_SUBSAMPLING:
        jpeg_mcu_w = 16U;
        jpeg_mcu_h = 8U;
        jpeg_mcu_size = 256U;
        break;

    case JPEG_444_SUBSAMPLING:
        jpeg_mcu_w = 8U;
        jpeg_mcu_h = 8U;
        jpeg_mcu_size = 192U;
        break;

    default:jpeg_error = 1;return;
    }

    if (JPEG_Info.ImageWidth == 0U ||
        JPEG_Info.ImageHeight == 0U ||
        JPEG_POS_X > DISP_W ||
        JPEG_POS_Y > DISP_H ||
        JPEG_Info.ImageWidth > DISP_W - JPEG_POS_X ||
        JPEG_Info.ImageHeight > DISP_H - JPEG_POS_Y) 
    {
        jpeg_error = 1;
        return;
    }
		
    /* MCU格式的缓冲区格式长宽向上取整，比如说227×149的图片，它转换为MCU缓冲区的格式，单个块16x16
		   假如按传统的除法那它必然是227/16=14.18,149/16=9.31,很明显无法整除所以需要向上对齐于是MCU缓冲区格式为15x10
		*/
    jpeg_mcu_cols=(JPEG_Info.ImageWidth+jpeg_mcu_w-1U)/jpeg_mcu_w;
    jpeg_mcu_rows=(JPEG_Info.ImageHeight+jpeg_mcu_h-1U)/jpeg_mcu_h;

    jpeg_draw_x = JPEG_POS_X;
    jpeg_draw_y = JPEG_POS_Y;
}

void HAL_JPEG_GetDataCallback(JPEG_HandleTypeDef *hjpeg,
                              uint32_t NbDecodedData)
{
    FSIZE_t file_pos;
    UINT bytes_read = 0;
    FRESULT fr;

    if (NbDecodedData > jpeg_input_len) 
    {
        jpeg_error = 1;
        HAL_JPEG_ConfigInputBuffer(hjpeg, JPEG_InBuffer, 0U);
        return;
    }

    /* 如果当前输入块没有被完全消耗，回退到未消耗数据的位置 */
    if (NbDecodedData != jpeg_input_len) 
    {
        file_pos = f_tell(&jpeg_file);

        if (file_pos < jpeg_input_len) 
        {
            jpeg_error = 1;
            HAL_JPEG_ConfigInputBuffer(hjpeg, JPEG_InBuffer, 0U);
            return;
        }

        file_pos = file_pos - jpeg_input_len + NbDecodedData;

        fr = f_lseek(&jpeg_file, file_pos);
        if (fr != FR_OK) 
        {
            jpeg_error = 1;
            HAL_JPEG_ConfigInputBuffer(hjpeg, JPEG_InBuffer, 0U);
            return;
        }
    }

    fr = f_read(&jpeg_file, JPEG_InBuffer, sizeof(JPEG_InBuffer), &bytes_read);

    if (fr != FR_OK || bytes_read == 0U) 
    {
			jpeg_input_len = 0U;
			HAL_JPEG_ConfigInputBuffer(hjpeg, JPEG_InBuffer, 0U);
			return;
    }

    jpeg_input_len = bytes_read;
    HAL_JPEG_ConfigInputBuffer(hjpeg, JPEG_InBuffer, jpeg_input_len);
}

void HAL_JPEG_DataReadyCallback(JPEG_HandleTypeDef *hjpeg,
                                uint8_t *pDataOut,
                                uint32_t OutDataLength)
{
    uint32_t offset;
    uint32_t col;
    uint32_t row;
    uint32_t img_x;
    uint32_t img_y;
    uint32_t x;
    uint32_t y;
    uint32_t valid_w;
    uint32_t valid_h;

    if (!jpeg_error && jpeg_input_len) 
		{
				for (offset = 0; offset < OutDataLength; offset += jpeg_mcu_size) 
			 {
					if(jpeg_mcu_index>=(jpeg_mcu_cols*jpeg_mcu_rows)) break;

					col = jpeg_mcu_index % jpeg_mcu_cols;
					row = jpeg_mcu_index / jpeg_mcu_cols;

					img_x = col * jpeg_mcu_w;
					img_y = row * jpeg_mcu_h;

					/* 计算图片边缘 MCU 中真正属于图像的像素数，假如剩余长度大于一个mcu块的长宽则取整一块的mcu长宽 */
					valid_w = JPEG_Info.ImageWidth - img_x;
					valid_h = JPEG_Info.ImageHeight - img_y;
					if (valid_w > jpeg_mcu_w) valid_w = jpeg_mcu_w;
					if (valid_h > jpeg_mcu_h) valid_h = jpeg_mcu_h;

					x = jpeg_draw_x + img_x;
					y = jpeg_draw_y + img_y;

					ltdc_lcd_dma2d_ycbcr_rgb565
					(
						(const uint32_t *)(pDataOut + offset),
						(uint16_t)x,
						(uint16_t)valid_w,
						(uint16_t)y,
						(uint16_t)valid_h,
						JPEG_Info.ChromaSubsampling
					);

					jpeg_mcu_index++;
			}
    }

    HAL_JPEG_ConfigOutputBuffer(hjpeg, JPEG_OutBuffer,sizeof(JPEG_OutBuffer));
}

void HAL_JPEG_DecodeCpltCallback(JPEG_HandleTypeDef *hjpeg)
{
    jpeg_decode_done = 1;
}

void HAL_JPEG_ErrorCallback(JPEG_HandleTypeDef *hjpeg)
{
    jpeg_error = 1;
}

/**
  * @brief  This function handles JPEG interrupt request.
  * @param  None
  * @retval None
  */

void JPEG_IRQHandler(void)
{
  HAL_JPEG_IRQHandler(&JPEG_Handle);
}
