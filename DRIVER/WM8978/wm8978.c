#include "wm8978.h"
#include <string.h>

/* WM8978 register addresses */
#define WM8978_REG_RESET          0U
#define WM8978_REG_POWER1         1U
#define WM8978_REG_POWER2         2U
#define WM8978_REG_POWER3         3U
#define WM8978_REG_IFACE          4U
#define WM8978_REG_CLOCK          6U
#define WM8978_REG_SRATE          7U
#define WM8978_REG_DAC_CTRL       10U
#define WM8978_REG_DAC_VOL_L      11U
#define WM8978_REG_DAC_VOL_R      12U
#define WM8978_REG_BEEP           43U
#define WM8978_REG_OUTPUT_CTRL    49U
#define WM8978_REG_MIX_L          50U
#define WM8978_REG_MIX_R          51U
#define WM8978_REG_HP_VOL_L       52U
#define WM8978_REG_HP_VOL_R       53U
#define WM8978_REG_SPK_VOL_L      54U
#define WM8978_REG_SPK_VOL_R      55U

#define WM8978_DAC_SOFTMUTE       (1U << 6)
#define WM8978_DAC_OSR128         (1U << 3)
#define WM8978_OUTPUT_MUTE        (1U << 6)
#define WM8978_VOLUME_UPDATE      (1U << 8)

#define DMA_SXCR_EN               (1U << 0)
#define DMA_SXCR_DMEIE            (1U << 1)
#define DMA_SXCR_TEIE             (1U << 2)
#define DMA_SXCR_TCIE             (1U << 4)
#define DMA_SXCR_DBM              (1U << 18)
#define DMA_SXCR_CT               (1U << 19)
#define DMA_SXCR_ALL_FLAGS        0x0FC0U

#define WM8978_DMA_STOP_TIMEOUT   1000000U

__attribute__((section(".RAM_D2"))) static I2C_HandleTypeDef wm8978_i2c;
__attribute__((section(".RAM_D2"))) static I2S_HandleTypeDef wm8978_i2s;
__attribute__((section(".RAM_D2"))) static DMA_HandleTypeDef wm8978_tx_dma;

__attribute__((section(".RAM_D2"), aligned(32)))
static int16_t wm8978_tx_buffer[WM8978_TX_BUFFER_COUNT][WM8978_TX_BUFFER_HALFWORDS];

static uint16_t wm8978_reg_cache[58] =
{
    0x000,0x000,0x000,0x000,0x050,0x000,0x140,0x000,
    0x000,0x000,0x000,0x0FF,0x0FF,0x000,0x100,0x0FF,
    0x0FF,0x000,0x12C,0x02C,0x02C,0x02C,0x02C,0x000,
    0x032,0x000,0x000,0x000,0x000,0x000,0x000,0x000,
    0x038,0x00B,0x032,0x000,0x008,0x00C,0x093,0x0E9,
    0x000,0x000,0x000,0x000,0x003,0x010,0x010,0x100,
    0x100,0x002,0x001,0x001,0x039,0x039,0x039,0x039,
    0x001,0x001
};

static volatile uint8_t wm8978_running;
static volatile uint8_t wm8978_free_buffer;
static volatile uint32_t wm8978_dma_error;
static volatile WM8978_BufferCallback wm8978_buffer_callback;
static uint8_t wm8978_codec_ready;
static uint8_t wm8978_muted = 1U;
static uint8_t wm8978_volume = 48U;
static uint8_t wm8978_dac_volume_l = 0xFFU;
static uint8_t wm8978_dac_volume_r = 0xFFU;
static uint32_t wm8978_sample_rate;
static WM8978_OutputType wm8978_output = WM8978_OUTPUT_SPEAKER;

static HAL_StatusTypeDef wm8978_i2c_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C2_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_10 | GPIO_PIN_11;
    gpio.Mode = GPIO_MODE_AF_OD;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF4_I2C2;
    HAL_GPIO_Init(GPIOB, &gpio);

    memset(&wm8978_i2c, 0, sizeof(wm8978_i2c));
    wm8978_i2c.Instance = I2C2;
    wm8978_i2c.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    wm8978_i2c.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    wm8978_i2c.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    wm8978_i2c.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    wm8978_i2c.Init.OwnAddress1 = 0;
    wm8978_i2c.Init.OwnAddress2 = 0;
    wm8978_i2c.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    wm8978_i2c.Init.Timing = 0xB0420F13;

    return HAL_I2C_Init(&wm8978_i2c);
}

static void wm8978_i2s_gpio_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_SPI2_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 |GPIO_PIN_15;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(GPIOB, &gpio);

    gpio.Pin = GPIO_PIN_6;
    HAL_GPIO_Init(GPIOC, &gpio);
}

static HAL_StatusTypeDef wm8978_i2s_init(uint32_t sample_rate)
{
    if (wm8978_i2s.Instance != NULL)
    {
        if (HAL_I2S_DeInit(&wm8978_i2s) != HAL_OK) return HAL_ERROR;
    }

    memset(&wm8978_i2s, 0, sizeof(wm8978_i2s));
    wm8978_i2s_gpio_init();

    wm8978_i2s.Instance = SPI2;
    wm8978_i2s.Init.AudioFreq = sample_rate;
    wm8978_i2s.Init.CPOL = I2S_CPOL_LOW;
    wm8978_i2s.Init.Data24BitAlignment = I2S_DATA_24BIT_ALIGNMENT_RIGHT;
    wm8978_i2s.Init.DataFormat = I2S_DATAFORMAT_16B;
    wm8978_i2s.Init.FirstBit = I2S_FIRSTBIT_MSB;
    wm8978_i2s.Init.MasterKeepIOState = I2S_MASTER_KEEP_IO_STATE_DISABLE;
    wm8978_i2s.Init.MCLKOutput = I2S_MCLKOUTPUT_ENABLE;
    wm8978_i2s.Init.Mode = I2S_MODE_MASTER_TX;
    wm8978_i2s.Init.Standard = I2S_STANDARD_PHILIPS;
    wm8978_i2s.Init.WSInversion = I2S_WS_INVERSION_DISABLE;

    if (HAL_I2S_Init(&wm8978_i2s) != HAL_OK) return HAL_ERROR;

    SPI2->CFG1 |= SPI_CFG1_TXDMAEN;
    return HAL_OK;
}

static HAL_StatusTypeDef wm8978_dma_init(void)
{
    __HAL_RCC_DMA2_CLK_ENABLE();

    DMA2_Stream1->CR &= ~DMA_SXCR_EN;
    for (volatile uint32_t timeout = 0;(DMA2_Stream1->CR & DMA_SXCR_EN) && timeout < WM8978_DMA_STOP_TIMEOUT;timeout++);

    if (DMA2_Stream1->CR & DMA_SXCR_EN) return HAL_TIMEOUT;

    memset(&wm8978_tx_dma, 0, sizeof(wm8978_tx_dma));
    wm8978_tx_dma.Instance = DMA2_Stream1;
    wm8978_tx_dma.Init.Direction = DMA_MEMORY_TO_PERIPH;
    wm8978_tx_dma.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
    wm8978_tx_dma.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
    wm8978_tx_dma.Init.MemBurst = DMA_MBURST_SINGLE;
    wm8978_tx_dma.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    wm8978_tx_dma.Init.MemInc = DMA_MINC_ENABLE;
    wm8978_tx_dma.Init.Mode = DMA_CIRCULAR;
    wm8978_tx_dma.Init.PeriphBurst = DMA_PBURST_SINGLE;
    wm8978_tx_dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    wm8978_tx_dma.Init.PeriphInc = DMA_PINC_DISABLE;
    wm8978_tx_dma.Init.Priority = DMA_PRIORITY_VERY_HIGH;
    wm8978_tx_dma.Init.Request = DMA_REQUEST_SPI2_TX;

    if (HAL_DMA_Init(&wm8978_tx_dma) != HAL_OK) return HAL_ERROR;

    DMA2->LIFCR = DMA_SXCR_ALL_FLAGS;
    DMA2_Stream1->PAR = (uint32_t)&SPI2->TXDR;
    DMA2_Stream1->M0AR = (uint32_t)wm8978_tx_buffer[0];
    DMA2_Stream1->M1AR = (uint32_t)wm8978_tx_buffer[1];
    DMA2_Stream1->NDTR = WM8978_TX_BUFFER_HALFWORDS;
    DMA2_Stream1->CR |= DMA_SXCR_DBM | DMA_SXCR_TCIE | DMA_SXCR_TEIE | DMA_SXCR_DMEIE;
    DMA2_Stream1->CR &= ~DMA_SXCR_CT;
    DMA2_Stream1->FCR |= (1U << 7); /* FIFO error interrupt enable */

    HAL_NVIC_SetPriority(DMA2_Stream1_IRQn, 7, 0);
    HAL_NVIC_ClearPendingIRQ(DMA2_Stream1_IRQn);
    HAL_NVIC_EnableIRQ(DMA2_Stream1_IRQn);
    return HAL_OK;
}

HAL_StatusTypeDef wm8978_write_reg(uint8_t reg, uint16_t value)
{
    uint8_t data[2];
    HAL_StatusTypeDef status;

    data[0] = (uint8_t)((reg << 1) | ((value >> 8) & 0x01U));
    data[1] = (uint8_t)value;
	
	  for(int i=0;i<10;i++)
		{
			status = HAL_I2C_Master_Transmit(&wm8978_i2c,WM8978_I2C_ADDR_8BIT,data, 2, HAL_MAX_DELAY);
			if (status == HAL_OK)
			{
				wm8978_reg_cache[reg] = value;
				return HAL_OK;
			}
		}

    return status;
}

uint16_t wm8978_read_reg(uint8_t reg)
{
    if (reg >= 58U) return 0;
    return wm8978_reg_cache[reg];
}

HAL_StatusTypeDef wm8978_reset(void)
{
    HAL_StatusTypeDef status;

    status = wm8978_write_reg(WM8978_REG_RESET, 0);
    if (status != HAL_OK) return status;

    HAL_Delay(10);
    wm8978_reg_cache[0] = 0;
    return HAL_OK;
}

static uint8_t wm8978_get_sample_rate_code(uint32_t rate, uint8_t *code)
{
    switch (rate)
    {
        case 48000U:
        case 44100U: *code = 0U; break;
        case 32000U: *code = 1U; break;
        case 24000U:
        case 22050U: *code = 2U; break;
        case 16000U: *code = 3U; break;
        case 12000U:
        case 11025U: *code = 4U; break;
        case 8000U:  *code = 5U; break;
        default: return 0U;
    }
    return 1U;
}

static HAL_StatusTypeDef wm8978_write_output_volume(uint8_t mute)
{
    uint16_t value = (uint16_t)(wm8978_volume & 0x3FU);
    HAL_StatusTypeDef status;

    if (mute) value |= WM8978_OUTPUT_MUTE;

    if (wm8978_output == WM8978_OUTPUT_HEADPHONE || wm8978_output == WM8978_OUTPUT_BOTH)
    {
        status = wm8978_write_reg(WM8978_REG_HP_VOL_L, value);
        if (status != HAL_OK) return status;
        status = wm8978_write_reg(WM8978_REG_HP_VOL_R, value | WM8978_VOLUME_UPDATE);
        if (status != HAL_OK) return status;
    }

    if (wm8978_output == WM8978_OUTPUT_SPEAKER || wm8978_output == WM8978_OUTPUT_BOTH)
    {
        status = wm8978_write_reg(WM8978_REG_SPK_VOL_L, value);
        if (status != HAL_OK) return status;
        status = wm8978_write_reg(WM8978_REG_SPK_VOL_R, value | WM8978_VOLUME_UPDATE);
        if (status != HAL_OK) return status;
    }

    return HAL_OK;
}

static HAL_StatusTypeDef wm8978_codec_init(uint32_t sample_rate)
{
    uint8_t sr_code;
    HAL_StatusTypeDef status;

    if (!wm8978_get_sample_rate_code(sample_rate, &sr_code)) return HAL_ERROR;

    status = wm8978_reset();
    if (status != HAL_OK) return status;

    wm8978_muted = 1U;
    wm8978_volume = 48U; /* -9dB initial output level */
    wm8978_dac_volume_l = 0xFFU;
    wm8978_dac_volume_r = 0xFFU;

    /* First keep all analogue output pins muted. */
    if (wm8978_write_reg(WM8978_REG_HP_VOL_L, 0x079) != HAL_OK)  return HAL_ERROR;
    if (wm8978_write_reg(WM8978_REG_HP_VOL_R, 0x179) != HAL_OK)  return HAL_ERROR;
    if (wm8978_write_reg(WM8978_REG_SPK_VOL_L, 0x079) != HAL_OK) return HAL_ERROR;
    if (wm8978_write_reg(WM8978_REG_SPK_VOL_R, 0x179) != HAL_OK) return HAL_ERROR;

    /* Enable DACs and left/right mixers before starting the analogue bias. */
    if (wm8978_write_reg(WM8978_REG_POWER3, 0x00F) != HAL_OK)    return HAL_ERROR;

    /* BUFIOEN=1, VMIDSEL=11 (5k); wait for VMID to settle. */
    if (wm8978_write_reg(WM8978_REG_POWER1, 0x007) != HAL_OK)    return HAL_ERROR;
    HAL_Delay(500);

    /* BIASEN=1. No 1.5x speaker boost is used; BUFDCOPEN remains 0. */
    if (wm8978_write_reg(WM8978_REG_POWER1, 0x00F) != HAL_OK)    return HAL_ERROR;

    /* Philips I2S, 16-bit, codec is I2S slave; STM32 supplies MCLK/BCLK/WS. */
    if (wm8978_write_reg(WM8978_REG_IFACE, 0x010) != HAL_OK)     return HAL_ERROR;
    if (wm8978_write_reg(WM8978_REG_CLOCK, 0x000) != HAL_OK)     return HAL_ERROR;
    if (wm8978_write_reg(WM8978_REG_SRATE, (uint16_t)(sr_code << 1)) != HAL_OK) return HAL_ERROR;

    /* DACOSR128=1, SOFTMUTE=1 until audio_start(). */
    if (wm8978_write_reg(WM8978_REG_DAC_CTRL, WM8978_DAC_OSR128 | WM8978_DAC_SOFTMUTE) != HAL_OK)
        return HAL_ERROR;

    if (wm8978_write_reg(WM8978_REG_DAC_VOL_L, wm8978_dac_volume_l) != HAL_OK) return HAL_ERROR;
    if (wm8978_write_reg(WM8978_REG_DAC_VOL_R, 0x100U | wm8978_dac_volume_r) != HAL_OK) return HAL_ERROR;

    /* Route left DAC to left mixer, right DAC to right mixer. */
    if (wm8978_write_reg(WM8978_REG_MIX_L, 0x001) != HAL_OK) return HAL_ERROR;
    if (wm8978_write_reg(WM8978_REG_MIX_R, 0x001) != HAL_OK) return HAL_ERROR;

    /* Thermal shutdown enable, speaker boost remains disabled. */
    if (wm8978_write_reg(WM8978_REG_OUTPUT_CTRL, 0x002) != HAL_OK) return HAL_ERROR;

    if (wm8978_output == WM8978_OUTPUT_SPEAKER || wm8978_output == WM8978_OUTPUT_BOTH)
    {
        /* Invert ROUT2 for the BTL speaker connection used by the reference board. */
        if (wm8978_write_reg(WM8978_REG_BEEP, 0x010) != HAL_OK) return HAL_ERROR;
        /* Enable L/R mixer, DAC and both speaker output stages. */
        if (wm8978_write_reg(WM8978_REG_POWER3, 0x06F) != HAL_OK) return HAL_ERROR;
    }

    if (wm8978_output == WM8978_OUTPUT_HEADPHONE || wm8978_output == WM8978_OUTPUT_BOTH)
    {
        if (wm8978_write_reg(WM8978_REG_POWER2, 0x180) != HAL_OK) return HAL_ERROR;
    }

    if (wm8978_write_output_volume(1U) != HAL_OK) return HAL_ERROR;

    wm8978_sample_rate = sample_rate;
    return HAL_OK;
}

HAL_StatusTypeDef wm8978_init(uint32_t sample_rate, WM8978_OutputType output)
{
    if (output != WM8978_OUTPUT_SPEAKER &&
        output != WM8978_OUTPUT_HEADPHONE &&
        output != WM8978_OUTPUT_BOTH)
        return HAL_ERROR;

    if (wm8978_running) wm8978_audio_stop();

    wm8978_codec_ready = 0U;
    wm8978_running = 0U;
    wm8978_free_buffer = 0U;
    wm8978_dma_error = 0U;
    wm8978_buffer_callback = NULL;
    wm8978_output = output;

    memset(wm8978_tx_buffer, 0, sizeof(wm8978_tx_buffer));

    if (wm8978_i2c_init() != HAL_OK)              return HAL_ERROR;
    if (wm8978_i2s_init(sample_rate) != HAL_OK)   return HAL_ERROR;
    if (wm8978_dma_init() != HAL_OK)              return HAL_ERROR;
    if (wm8978_codec_init(sample_rate) != HAL_OK) return HAL_ERROR;

    wm8978_codec_ready = 1U;
    return HAL_OK;
}

void wm8978_deinit(void)
{
    wm8978_audio_stop();

    if (wm8978_i2c.Instance != NULL)
    {
        wm8978_write_reg(WM8978_REG_POWER1, 0x000);
        wm8978_write_reg(WM8978_REG_POWER2, 0x000);
        wm8978_write_reg(WM8978_REG_POWER3, 0x000);
        HAL_I2C_DeInit(&wm8978_i2c);
    }

    if (wm8978_i2s.Instance != NULL) HAL_I2S_DeInit(&wm8978_i2s);

    HAL_NVIC_DisableIRQ(DMA2_Stream1_IRQn);
    wm8978_codec_ready = 0U;
}

HAL_StatusTypeDef wm8978_set_sample_rate(uint32_t sample_rate)
{
    uint8_t sr_code;

    if (!wm8978_codec_ready)                                                    return HAL_ERROR;
    if (wm8978_running)                                                         return HAL_BUSY;
    if (!wm8978_get_sample_rate_code(sample_rate, &sr_code))                    return HAL_ERROR;

    if (wm8978_i2s_init(sample_rate) != HAL_OK)                                 return HAL_ERROR;
    if (wm8978_write_reg(WM8978_REG_SRATE, (uint16_t)(sr_code << 1)) != HAL_OK) return HAL_ERROR;

    wm8978_sample_rate = sample_rate;
    return HAL_OK;
}

HAL_StatusTypeDef wm8978_set_volume(uint8_t volume)
{
    if (!wm8978_codec_ready) return HAL_ERROR;
    if (volume > 63U) volume = 63U;

    wm8978_volume = volume;
    return wm8978_write_output_volume(wm8978_muted);
}

HAL_StatusTypeDef wm8978_set_volume_db(int8_t db)
{
    if (db < -57) db = -57;
    if (db > 6)   db = 6;
    return wm8978_set_volume((uint8_t)(db + 57));
}

HAL_StatusTypeDef wm8978_set_dac_volume(uint8_t left, uint8_t right)
{
    HAL_StatusTypeDef status;

    if (!wm8978_codec_ready) return HAL_ERROR;

    status = wm8978_write_reg(WM8978_REG_DAC_VOL_L, left);
    if (status != HAL_OK)    return status;
    status = wm8978_write_reg(WM8978_REG_DAC_VOL_R, 0x100U | right);
    if (status != HAL_OK)    return status;

    wm8978_dac_volume_l = left;
    wm8978_dac_volume_r = right;
    return HAL_OK;
}

HAL_StatusTypeDef wm8978_set_mute(uint8_t mute)
{
    HAL_StatusTypeDef status;
    uint16_t dac_ctrl = wm8978_read_reg(WM8978_REG_DAC_CTRL);

    if (!wm8978_codec_ready) return HAL_ERROR;

    mute = mute ? 1U : 0U;

    if (mute)
    {
        /* First mute analogue outputs, then engage the DAC soft-mute ramp. */
        if (wm8978_write_output_volume(1U) != HAL_OK) return HAL_ERROR;
        dac_ctrl |= WM8978_DAC_SOFTMUTE;
        status = wm8978_write_reg(WM8978_REG_DAC_CTRL, dac_ctrl);
        if (status == HAL_OK) wm8978_muted = 1U;
        return status;
    }

    /* Release DAC soft mute first, then unmute the analogue output stages. */
    dac_ctrl &= (uint16_t)~WM8978_DAC_SOFTMUTE;
    status = wm8978_write_reg(WM8978_REG_DAC_CTRL, dac_ctrl);
    if (status != HAL_OK) return status;
    HAL_Delay(5);
    status = wm8978_write_output_volume(0U);
    if (status == HAL_OK) wm8978_muted = 0U;
    return status;
}

HAL_StatusTypeDef wm8978_set_output(WM8978_OutputType output)
{
    if (!wm8978_codec_ready) return HAL_ERROR;
    if (wm8978_running)      return HAL_BUSY;
    if (output != WM8978_OUTPUT_SPEAKER &&
        output != WM8978_OUTPUT_HEADPHONE &&
        output != WM8978_OUTPUT_BOTH)
        return HAL_ERROR;

    if (wm8978_set_mute(1U) != HAL_OK)                        return HAL_ERROR;

    if (wm8978_write_reg(WM8978_REG_POWER2, 0x000) != HAL_OK) return HAL_ERROR;
    if (wm8978_write_reg(WM8978_REG_POWER3, 0x00F) != HAL_OK) return HAL_ERROR;

    wm8978_output = output;

    if (output == WM8978_OUTPUT_SPEAKER || output == WM8978_OUTPUT_BOTH)
    {
        if (wm8978_write_reg(WM8978_REG_BEEP, 0x010) != HAL_OK)   return HAL_ERROR;
        if (wm8978_write_reg(WM8978_REG_POWER3, 0x06F) != HAL_OK) return HAL_ERROR;
    }

    if (output == WM8978_OUTPUT_HEADPHONE || output == WM8978_OUTPUT_BOTH)
    {
        if (wm8978_write_reg(WM8978_REG_POWER2, 0x180) != HAL_OK) return HAL_ERROR;
    }

    return wm8978_write_output_volume(1U);
}

int16_t *wm8978_audio_get_buffer(uint8_t index)
{
    if (index >= WM8978_TX_BUFFER_COUNT) return NULL;
    return wm8978_tx_buffer[index];
}

uint32_t wm8978_audio_get_buffer_halfwords(void)
{
    return WM8978_TX_BUFFER_HALFWORDS;
}

uint32_t wm8978_audio_get_buffer_frames(void)
{
    return WM8978_TX_BUFFER_STEREO_FRAMES;
}

uint8_t wm8978_audio_get_free_buffer_index(void)
{
    return wm8978_free_buffer;
}

HAL_StatusTypeDef wm8978_audio_submit_buffer(uint8_t index)
{
    if (index >= WM8978_TX_BUFFER_COUNT) return HAL_ERROR;

    if (wm8978_running)
    {
        uint8_t current = (DMA2_Stream1->CR & DMA_SXCR_CT) ? 1U : 0U;
        if (current == index) return HAL_BUSY;
    }

#if (__DCACHE_PRESENT == 1U)
    if (SCB->CCR & SCB_CCR_DC_Msk)
    {
        SCB_CleanDCache_by_Addr((uint32_t *)wm8978_tx_buffer[index],
                                (int32_t)sizeof(wm8978_tx_buffer[index]));
        __DSB();
    }
#endif
    return HAL_OK;
}

void wm8978_audio_set_callback(WM8978_BufferCallback callback)
{
    wm8978_buffer_callback = callback;
}

HAL_StatusTypeDef wm8978_audio_start(void)
{
    if (!wm8978_codec_ready) return HAL_ERROR;
    if (wm8978_running)      return HAL_OK;

    if (wm8978_audio_submit_buffer(0U) != HAL_OK) return HAL_ERROR;
    if (wm8978_audio_submit_buffer(1U) != HAL_OK) return HAL_ERROR;

    DMA2_Stream1->CR &= ~DMA_SXCR_EN;
    for (volatile uint32_t timeout = 0;(DMA2_Stream1->CR & DMA_SXCR_EN) && timeout < WM8978_DMA_STOP_TIMEOUT;timeout++);

    if (DMA2_Stream1->CR & DMA_SXCR_EN) return HAL_TIMEOUT;

    DMA2->LIFCR = DMA_SXCR_ALL_FLAGS;
    DMA2_Stream1->PAR = (uint32_t)&SPI2->TXDR;
    DMA2_Stream1->M0AR = (uint32_t)wm8978_tx_buffer[0];
    DMA2_Stream1->M1AR = (uint32_t)wm8978_tx_buffer[1];
    DMA2_Stream1->NDTR = WM8978_TX_BUFFER_HALFWORDS;
    DMA2_Stream1->CR &= ~DMA_SXCR_CT;
    DMA2_Stream1->CR |= DMA_SXCR_DBM | DMA_SXCR_TCIE | DMA_SXCR_TEIE | DMA_SXCR_DMEIE;

    wm8978_free_buffer = 0U;
    wm8978_dma_error = 0U;

    SPI2->CFG1 |= SPI_CFG1_TXDMAEN;
    DMA2_Stream1->CR |= DMA_SXCR_EN;
    SPI2->CR1 |= SPI_CR1_SPE;
    SPI2->CR1 |= SPI_CR1_CSTART;
    wm8978_running = 1U;

    if (wm8978_set_mute(0U) != HAL_OK)
    {
        wm8978_audio_stop();
        return HAL_ERROR;
    }

    return HAL_OK;
}

void wm8978_audio_stop(void)
{
    if (!wm8978_codec_ready) return;

    wm8978_set_mute(1U);
    HAL_Delay(10);

    SPI2->CFG1 &= ~SPI_CFG1_TXDMAEN;
    DMA2_Stream1->CR &= ~DMA_SXCR_EN;

    for(volatile uint32_t timeout=0;(DMA2_Stream1->CR & DMA_SXCR_EN) && timeout<WM8978_DMA_STOP_TIMEOUT;timeout++);

    SPI2->CR1 &= ~SPI_CR1_CSTART;
    SPI2->CR1 &= ~SPI_CR1_SPE;
    wm8978_running = 0U;
}

uint8_t wm8978_audio_is_running(void)
{
    return wm8978_running;
}

uint32_t wm8978_audio_get_dma_error(void)
{
    return wm8978_dma_error;
}

void DMA2_Stream1_IRQHandler(void)
{
    uint32_t status = DMA2->LISR;
    uint32_t flags = status & DMA_SXCR_ALL_FLAGS;

    if (flags == 0U) return;

    DMA2->LIFCR = flags;

    if (flags & ((1U << 6) | (1U << 8) | (1U << 9)))
        wm8978_dma_error |= flags & ((1U << 6) | (1U << 8) | (1U << 9));

    if ((flags & (1U << 11)) && wm8978_running)
    {
        /* CT=1: DMA is now using M1, so M0 has just become free. */
        wm8978_free_buffer = (DMA2_Stream1->CR & DMA_SXCR_CT) ? 0U : 1U;

        if (wm8978_buffer_callback != NULL)
        {
            uint8_t index = wm8978_free_buffer;
            wm8978_buffer_callback(index, wm8978_tx_buffer[index],WM8978_TX_BUFFER_HALFWORDS);
        }
    }
}
