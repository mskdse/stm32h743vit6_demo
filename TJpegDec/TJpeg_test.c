#include "TJpeg_test.h"

__attribute__((section(".RAM_D1"),aligned(4)))static FIL     file;
__attribute__((section(".RAM_D1"),aligned(4)))static JDEC    jdec;
__attribute__((section(".RAM_D1"),aligned(4)))static uint8_t jpeg_work_buf[4096]; // 工作区，建议至少 3100 字节

// 从 FatFs 文件读取 JPEG 数据
static size_t fatfs_in_func(JDEC* jd, uint8_t* buff, size_t nbyte) 
{
    FIL* file = (FIL*)jd->device; // 取出传入的 FIL 指针
    UINT bytes_read = 0;
    
    if (buff) 
	  {
			// 正常读取数据到缓冲区
			if (f_read(file, buff, nbyte, &bytes_read) != FR_OK)
			{
				return 0; // 读取失败返回 0
			}
			return bytes_read; // 返回实际读取的字节数
    } 
	  else 
	  {
			// buff 为 NULL 时，表示 TJpgDec 希望跳过 nbyte 字节
			// 使用 f_lseek 移动文件指针
			FSIZE_t current = f_tell(file);
			if (f_lseek(file, current + nbyte) != FR_OK)
			{
				return 0; // 定位失败
			}
			return nbyte; // 跳过成功
    }
}

static int lcd_out_func(JDEC* jd, void* bitmap, JRECT* rect) 
{ 	
	 drvp_fmc_lcd_set_wid(rect->left,rect->right,rect->top,rect->bottom);
	 drvp_fmc_lcd_writebuf((uint16_t *)bitmap,(uint16_t)((rect->right-rect->left+1)*(rect->bottom-rect->top+1)));
	 return 1; // 返回 1 继续解码，返回 0 会中止解码[reference:4]
}

void TJpeg_Dec_Test(const char* filename)
{
	JRESULT res;

	memset(&file,0,sizeof(FIL));
	memset(&jdec,0,sizeof(JDEC));
	memset(jpeg_work_buf,0,sizeof(jpeg_work_buf));

	// 1. 用 FatFs 打开文件
	if (f_open(&file, filename, FA_READ) != FR_OK)  return; // 文件打开失败

	// 2. 准备解码，将 &file 作为设备指针传入
	res = jd_prepare(&jdec, fatfs_in_func, jpeg_work_buf, sizeof(jpeg_work_buf), &file);
	if (res != JDR_OK) 
	{
		f_close(&file);
		return; // JPEG 格式不支持或文件损坏
	}

	// 3. 执行解码，输出回调函数负责将像素刷到屏幕
	// 第3个参数可选缩放比例：0=1/1, 1=1/2, 2=1/4, 3=1/8
	res = jd_decomp(&jdec, lcd_out_func, 0);
	
	// 4. 无论成败，关闭文件
	f_close(&file);
}
