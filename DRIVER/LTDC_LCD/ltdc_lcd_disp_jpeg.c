#include "ltdc_lcd_disp_jpeg.h"
#include "image_320_240_jpg.h"

#define DISP_W   (LCD_WIN_X1 - LCD_WIN_X0)
#define DISP_H   (LCD_WIN_Y1 - LCD_WIN_Y0)

extern uint16_t LTDC_DISPLAY[((LCD_WIN_X1-LCD_WIN_X0)*(LCD_WIN_Y1-LCD_WIN_Y0))]; 

__attribute__((section (".RAM_D2")))static JPEG_HandleTypeDef JPEG_Handle;
__attribute__((section (".RAM_D2")))static JPEG_ConfTypeDef   JPEG_Info;
__attribute__((section (".RAM_D2")))static volatile uint32_t  filecount;
__attribute__((section (".RAM_D2")))static volatile uint8_t   filecount_end;
__attribute__((section (".RAM_D2")))static uint32_t           FrameBufferAddress;
#define CHUNK_SIZE_IN  ((uint32_t)(4096)) 
#define CHUNK_SIZE_OUT ((uint32_t)(64 * 1024))

void ltdc_lcd_disp_jpeg_init(void)
{
	/* Enable JPEG clock */
  __HAL_RCC_JPGDECEN_CLK_ENABLE();
	
	memset(&JPEG_Handle,0,sizeof(JPEG_HandleTypeDef));
	memset(&JPEG_Info,0,sizeof(JPEG_ConfTypeDef));
	filecount=0;
	filecount_end=0;
	
	JPEG_Handle.Instance=JPEG;
  HAL_JPEG_Init(&JPEG_Handle);
	
	FrameBufferAddress=(uint32_t)LTDC_DISPLAY;
	filecount+=CHUNK_SIZE_IN;
	HAL_JPEG_Decode(&JPEG_Handle,(uint8_t*)image_320_240_jpg,IMAGE_320_240_JPG_SIZE,(uint8_t*)FrameBufferAddress,IMAGE_320_240_JPG_SIZE,HAL_MAX_DELAY);
	HAL_JPEG_GetInfo(&JPEG_Handle, &JPEG_Info);
	ltdc_lcd_dma2d_ycbcr_rgb565((const uint16_t*)FrameBufferAddress,0,JPEG_Info.ImageWidth,0,JPEG_Info.ImageHeight,JPEG_Info.ChromaSubsampling);
}

//void HAL_JPEG_GetDataCallback(JPEG_HandleTypeDef *hjpeg, uint32_t NbDecodedData)
//{
//	//HAL_JPEG_ConfigInputBuffer(&JPEG_Handle,,);
//}

//void HAL_JPEG_DataReadyCallback(JPEG_HandleTypeDef *hjpeg, uint8_t *pDataOut, uint32_t OutDataLength)
//{
////	FrameBufferAddress+=OutDataLength;
////	HAL_JPEG_ConfigOutputBuffer(&JPEG_Handle,(uint8_t*)FrameBufferAddress,CHUNK_SIZE_OUT);
//}

//void HAL_JPEG_DecodeCpltCallback(JPEG_HandleTypeDef *hjpeg)
//{
////	filecount_end=1;
//}
