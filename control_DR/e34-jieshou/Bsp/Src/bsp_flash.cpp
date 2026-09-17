#include "bsp.h"

#include "stm32g4xx_hal.h"

#include <string.h>

static uint8_t Bsp_FlashPageFromAddress(uint32_t address, uint32_t *page)
{
    if ((page == nullptr) || (address < FLASH_BASE) ||
        ((address % FLASH_PAGE_SIZE) != 0U))
    {
        return 0U;
    }

    *page = (address - FLASH_BASE) / FLASH_PAGE_SIZE;
    return 1U;
}

extern "C" uint8_t Bsp_FlashErasePages(uint32_t address, uint32_t length)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t page = 0U;
    uint32_t page_error = 0U;
    HAL_StatusTypeDef status;
    uint8_t ok = 0U;

    if ((length == 0U) || ((length % FLASH_PAGE_SIZE) != 0U) ||
        (Bsp_FlashPageFromAddress(address, &page) == 0U))
    {
        return 0U;
    }

    if (HAL_FLASH_Unlock() != HAL_OK)
    {
        return 0U;
    }

    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.Banks = FLASH_BANK_1;
    erase.Page = page;
    erase.NbPages = length / FLASH_PAGE_SIZE;

    __disable_irq();
    status = HAL_FLASHEx_Erase(&erase, &page_error);
    __enable_irq();

    if ((status == HAL_OK) && (page_error == 0xFFFFFFFFU))
    {
        ok = 1U;
    }

    (void)HAL_FLASH_Lock();
    return ok;
}

extern "C" uint8_t Bsp_FlashProgram(uint32_t address, const uint8_t *data, uint32_t length)
{
    uint32_t offset;
    uint8_t ok = 1U;

    if ((data == nullptr) || (length == 0U) || ((length % 8U) != 0U) ||
        ((address % 8U) != 0U))
    {
        return 0U;
    }

    if (HAL_FLASH_Unlock() != HAL_OK)
    {
        return 0U;
    }

    for (offset = 0U; offset < length; offset += 8U)
    {
        uint64_t doubleword;

        memcpy(&doubleword, data + offset, sizeof(doubleword));
        __disable_irq();
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, address + offset,
                              doubleword) != HAL_OK)
        {
            ok = 0U;
            __enable_irq();
            break;
        }
        __enable_irq();
    }

    (void)HAL_FLASH_Lock();
    return ok;
}
