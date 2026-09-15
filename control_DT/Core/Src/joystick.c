/**
 * @file joystick.c
 * @brief 双摇杆四轴采样、滤波、中心校准和数值映射。
 *
 * 每 10 ms 由 Joystick_Update() 读取四路 ADC 原始值。每轴经过一阶
 * 低通滤波后参与中心校准和 -100～+100 映射：上电后保持摇杆居中约
 * 320 ms，程序会用 32 个滤波样本的平均值作为各轴零点。
 */
#include "joystick.h"
#include "adc.h"
#include "debug.h"

#define JOYSTICK_ADC_MAX_VALUE             4095U
#define JOYSTICK_NOMINAL_CENTER             2048U
#define JOYSTICK_FILTER_DIVISOR             4U
#define JOYSTICK_CALIBRATION_SAMPLE_COUNT   32U
#define JOYSTICK_CENTER_MIN_VALUE           512U
#define JOYSTICK_CENTER_MAX_VALUE           3583U

static uint32_t calibration_sum_left_y;
static uint32_t calibration_sum_left_x;
static uint32_t calibration_sum_right_y;
static uint32_t calibration_sum_right_x;
static uint8_t calibration_sample_count;
static uint8_t filter_initialized;

static int16_t Joystick_ClampValue(int16_t value)
{
  if (value > 100)
  {
    return 100;
  }
  if (value < -100)
  {
    return -100;
  }

  return value;
}

/*
 * 一阶低通滤波：保留约 75% 的历史值，叠加约 25% 的新采样值。
 * 用于抑制摇杆电位器抖动和 ADC 量化噪声；该算法不改变有效量程。
 */
static uint16_t Joystick_FilterSample(uint16_t previous, uint16_t sample)
{
  int32_t delta = (int32_t)sample - (int32_t)previous;

  /* First-order low-pass filter: 75% old sample + 25% new sample. */
  if (delta > 0)
  {
    return (uint16_t)((int32_t)previous + (delta + (int32_t)JOYSTICK_FILTER_DIVISOR - 1) /
                                              (int32_t)JOYSTICK_FILTER_DIVISOR);
  }
  if (delta < 0)
  {
    return (uint16_t)((int32_t)previous + (delta - ((int32_t)JOYSTICK_FILTER_DIVISOR - 1)) /
                                              (int32_t)JOYSTICK_FILTER_DIVISOR);
  }

  return previous;
}

static void Joystick_UpdateFilter(void)
{
  if (filter_initialized == 0U)
  {
    debug.joystick.filtered = debug.joystick.raw;
    filter_initialized = 1U;
    return;
  }

  debug.joystick.filtered.left_y = Joystick_FilterSample(debug.joystick.filtered.left_y, debug.joystick.raw.left_y);
  debug.joystick.filtered.left_x = Joystick_FilterSample(debug.joystick.filtered.left_x, debug.joystick.raw.left_x);
  debug.joystick.filtered.right_y = Joystick_FilterSample(debug.joystick.filtered.right_y, debug.joystick.raw.right_y);
  debug.joystick.filtered.right_x = Joystick_FilterSample(debug.joystick.filtered.right_x, debug.joystick.raw.right_x);
}

/*
 * 以中心校准值为 0 点作分段线性映射：ADC 下端映射至 -100，
 * 上端映射至 +100。中心两侧分别使用各自的剩余量程，保证两端
 * 都能达到满量程。
 */
static int16_t Joystick_ScaleWithCenter(uint16_t raw_value, uint16_t center_value)
{
  uint32_t distance;
  uint32_t span;
  int16_t value;

  if (raw_value > JOYSTICK_ADC_MAX_VALUE)
  {
    raw_value = JOYSTICK_ADC_MAX_VALUE;
  }

  if (center_value == 0U || center_value >= JOYSTICK_ADC_MAX_VALUE)
  {
    center_value = JOYSTICK_NOMINAL_CENTER;
  }

  if (raw_value >= center_value)
  {
    distance = (uint32_t)raw_value - center_value;
    span = JOYSTICK_ADC_MAX_VALUE - center_value;
    value = (int16_t)((distance * 100U + span / 2U) / span);
  }
  else
  {
    distance = (uint32_t)center_value - raw_value;
    span = center_value;
    value = -(int16_t)((distance * 100U + span / 2U) / span);
  }

  return Joystick_ClampValue(value);
}

int16_t Joystick_ScaleRaw(uint16_t raw_value)
{
  return Joystick_ScaleWithCenter(raw_value, JOYSTICK_NOMINAL_CENTER);
}

static void Joystick_ClearValues(void)
{
  debug.joystick.value.left_y = 0;
  debug.joystick.value.left_x = 0;
  debug.joystick.value.right_y = 0;
  debug.joystick.value.right_x = 0;
}

static void Joystick_UpdateScaledValues(void)
{
  debug.joystick.value.left_y = Joystick_ScaleWithCenter(debug.joystick.filtered.left_y,
                                                     debug.joystick.calibration.left_y_center);
  debug.joystick.value.left_x = Joystick_ScaleWithCenter(debug.joystick.filtered.left_x,
                                                     debug.joystick.calibration.left_x_center);
  debug.joystick.value.right_y = Joystick_ScaleWithCenter(debug.joystick.filtered.right_y,
                                                      debug.joystick.calibration.right_y_center);
  debug.joystick.value.right_x = Joystick_ScaleWithCenter(debug.joystick.filtered.right_x,
                                                      debug.joystick.calibration.right_x_center);
}

static uint8_t Joystick_CentersAreValid(void)
{
  return (debug.joystick.calibration.left_y_center >= JOYSTICK_CENTER_MIN_VALUE &&
          debug.joystick.calibration.left_y_center <= JOYSTICK_CENTER_MAX_VALUE &&
          debug.joystick.calibration.left_x_center >= JOYSTICK_CENTER_MIN_VALUE &&
          debug.joystick.calibration.left_x_center <= JOYSTICK_CENTER_MAX_VALUE &&
          debug.joystick.calibration.right_y_center >= JOYSTICK_CENTER_MIN_VALUE &&
          debug.joystick.calibration.right_y_center <= JOYSTICK_CENTER_MAX_VALUE &&
          debug.joystick.calibration.right_x_center >= JOYSTICK_CENTER_MIN_VALUE &&
          debug.joystick.calibration.right_x_center <= JOYSTICK_CENTER_MAX_VALUE);
}

/*
 * 中心校准：启动后累计 32 个已滤波样本，取平均值作为每个轴的零点。
 * 校准期间映射输出保持为 0；若任一中心值超出合理 ADC 范围，则本次
 * 校准无效，调用 Joystick_StartCalibration() 可重新开始。
 */
static void Joystick_ProcessCalibration(void)
{
  if (debug.joystick.calibration.in_progress == 0U)
  {
    return;
  }

  calibration_sum_left_y += debug.joystick.filtered.left_y;
  calibration_sum_left_x += debug.joystick.filtered.left_x;
  calibration_sum_right_y += debug.joystick.filtered.right_y;
  calibration_sum_right_x += debug.joystick.filtered.right_x;
  calibration_sample_count++;

  if (calibration_sample_count < JOYSTICK_CALIBRATION_SAMPLE_COUNT)
  {
    Joystick_ClearValues();
    return;
  }

  debug.joystick.calibration.left_y_center =
      (uint16_t)(calibration_sum_left_y / JOYSTICK_CALIBRATION_SAMPLE_COUNT);
  debug.joystick.calibration.left_x_center =
      (uint16_t)(calibration_sum_left_x / JOYSTICK_CALIBRATION_SAMPLE_COUNT);
  debug.joystick.calibration.right_y_center =
      (uint16_t)(calibration_sum_right_y / JOYSTICK_CALIBRATION_SAMPLE_COUNT);
  debug.joystick.calibration.right_x_center =
      (uint16_t)(calibration_sum_right_x / JOYSTICK_CALIBRATION_SAMPLE_COUNT);

  debug.joystick.calibration.in_progress = 0U;
  debug.joystick.calibration.valid = Joystick_CentersAreValid();

  if (debug.joystick.calibration.valid == 0U)
  {
    Joystick_ClearValues();
  }
}

void Joystick_StartCalibration(void)
{
  calibration_sum_left_y = 0U;
  calibration_sum_left_x = 0U;
  calibration_sum_right_y = 0U;
  calibration_sum_right_x = 0U;
  calibration_sample_count = 0U;

  debug.joystick.calibration.in_progress = 1U;
  debug.joystick.calibration.valid = 0U;
  Joystick_ClearValues();
}

static HAL_StatusTypeDef Joystick_ReadAdc1(void)
{
  HAL_StatusTypeDef status;

  status = HAL_ADC_Start(&hadc1);
  if (status != HAL_OK)
  {
    return status;
  }

  status = HAL_ADC_PollForConversion(&hadc1, 10U);
  if (status == HAL_OK)
  {
    /* ADC1 rank 1 / PA0 is the physical left X axis. */
    debug.joystick.raw.left_x = (uint16_t)HAL_ADC_GetValue(&hadc1);
    status = HAL_ADC_PollForConversion(&hadc1, 10U);
  }
  if (status == HAL_OK)
  {
    /* ADC1 rank 2 / PA1 is the physical left Y axis. */
    debug.joystick.raw.left_y = (uint16_t)HAL_ADC_GetValue(&hadc1);
    status = HAL_ADC_PollForConversion(&hadc1, 10U);
  }
  if (status == HAL_OK)
  {
    /* ADC1 rank 3 / PA2 is the physical right X axis. */
    debug.joystick.raw.right_x = (uint16_t)HAL_ADC_GetValue(&hadc1);
  }

  if (HAL_ADC_Stop(&hadc1) != HAL_OK && status == HAL_OK)
  {
    status = HAL_ERROR;
  }

  return status;
}

static HAL_StatusTypeDef Joystick_ReadAdc2(void)
{
  HAL_StatusTypeDef status;

  status = HAL_ADC_Start(&hadc2);
  if (status != HAL_OK)
  {
    return status;
  }

  status = HAL_ADC_PollForConversion(&hadc2, 10U);
  if (status == HAL_OK)
  {
    debug.joystick.raw.right_y = (uint16_t)HAL_ADC_GetValue(&hadc2);
  }

  if (HAL_ADC_Stop(&hadc2) != HAL_OK && status == HAL_OK)
  {
    status = HAL_ERROR;
  }

  return status;
}

HAL_StatusTypeDef Joystick_Init(void)
{
  HAL_StatusTypeDef status;

  status = HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
  if (status != HAL_OK)
  {
    return status;
  }

  status = HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED);
  if (status != HAL_OK)
  {
    return status;
  }

  Joystick_StartCalibration();
  return HAL_OK;
}

HAL_StatusTypeDef Joystick_Update(void)
{
  HAL_StatusTypeDef status = Joystick_ReadAdc1();

  if (status != HAL_OK)
  {
    return status;
  }

  status = Joystick_ReadAdc2();
  if (status != HAL_OK)
  {
    return status;
  }

  Joystick_UpdateFilter();
  Joystick_ProcessCalibration();

  if (debug.joystick.calibration.valid != 0U)
  {
    Joystick_UpdateScaledValues();
  }

  return HAL_OK;
}
