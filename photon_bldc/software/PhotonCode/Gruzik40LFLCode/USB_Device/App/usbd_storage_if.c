/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : usbd_storage_if.c
  * @version        : v3.0_Cube
  * @brief          : Memory management layer.
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "usbd_storage_if.h"

/* USER CODE BEGIN INCLUDE */
#include "string.h"
#include "main.h"
#include "spi.h"
#include <stdint.h>
#include <stdbool.h>
/* USER CODE END INCLUDE */

/* USER CODE BEGIN PV */
/* --- Flash geometry --- */
#define FLASH_PAGE_SIZE      256u
#define FLASH_SECTOR_SIZE    4096u
/* zwiększamy timeout, sector erase może trwać długo na niektórych chipach */
#define FLASH_TIMEOUT_MS     30000u

/* USB MSC geometry (exposed to host) */
#define STORAGE_LUN_NBR      1
#define STORAGE_BLK_SIZ      0x200      /* 512 B */
#define STORAGE_BLK_NBR      0x4000     /* 16384 * 512B = 8MiB */

/* Read chunk max */
#define READ_CHUNK_MAX       4096U

/* Flash commands */
#define CMD_READ_DATA        0x03
#define CMD_PAGE_PROGRAM     0x02
#define CMD_WRITE_ENABLE     0x06
#define CMD_READ_STATUS      0x05
#define CMD_SECTOR_ERASE     0x20
#define CMD_JEDEC_ID         0x9F

/* sector cache */
static uint8_t sector_cache_buf[FLASH_SECTOR_SIZE];
static uint32_t sector_cache_addr = 0xFFFFFFFF;
static bool sector_cache_dirty = false;

/* temporary buffer */
static uint8_t sector_buf[FLASH_SECTOR_SIZE];

/* SPI handle */
extern SPI_HandleTypeDef hspi2;

/* Debug prints: odkomentuj define żeby włączyć printf debugu (musisz mieć UART printf) */
/* #define STORAGE_DBG */
#ifdef STORAGE_DBG
  #include <stdio.h>
  #define DBG_PRINTF(...) printf(__VA_ARGS__)
#else
  #define DBG_PRINTF(...) (void)0
#endif
/* USER CODE END PV */

/* CS helpers */
static inline void FLASH_CS_LOW(void)  { HAL_GPIO_WritePin(SPI2_CS2_GPIO_Port, SPI2_CS2_Pin, GPIO_PIN_RESET); }
static inline void FLASH_CS_HIGH(void) { HAL_GPIO_WritePin(SPI2_CS2_GPIO_Port, SPI2_CS2_Pin, GPIO_PIN_SET); }

/* Forward decl */
static uint8_t FLASH_ReadStatus(void);
static int FLASH_WaitUntilReady(uint32_t timeout_ms);
static int FLASH_WriteEnable(void);
static int FLASH_ReadData(uint32_t addr, uint8_t *buf, uint32_t len);
static int FLASH_PageProgram(uint32_t addr, const uint8_t *buf, uint32_t len);
static int FLASH_ProgramBuffer(uint32_t addr, const uint8_t *buf, uint32_t len);
static int FLASH_SectorErase(uint32_t addr);

/* cache helpers */
static int STORAGE_FlushSectorCache(void);
static int STORAGE_LoadSectorToCache(uint32_t sector_addr);

/* public sync */
int STORAGE_Sync(void);

/* USER CODE BEGIN 1 */
static uint8_t FLASH_ReadStatus(void)
{
    uint8_t cmd = CMD_READ_STATUS;
    uint8_t status = 0xFF;
    FLASH_CS_LOW();
    HAL_SPI_Transmit(&hspi2, &cmd, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive(&hspi2, &status, 1, HAL_MAX_DELAY);
    FLASH_CS_HIGH();
    return status;
}

static int FLASH_WaitUntilReady(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    while ((FLASH_ReadStatus() & 0x01) != 0) {
        if ((HAL_GetTick() - start) >= timeout_ms) return -1;
        HAL_Delay(1);
    }
    return 0;
}

static int FLASH_WriteEnable(void)
{
    uint8_t cmd = CMD_WRITE_ENABLE;
    FLASH_CS_LOW();
    HAL_StatusTypeDef st = HAL_SPI_Transmit(&hspi2, &cmd, 1, HAL_MAX_DELAY);
    FLASH_CS_HIGH();
    return (st == HAL_OK) ? 0 : -1;
}

static int FLASH_ReadData(uint32_t addr, uint8_t *buf, uint32_t len)
{
    uint8_t cmd_addr[4];
    cmd_addr[0] = CMD_READ_DATA;
    cmd_addr[1] = (addr >> 16) & 0xFF;
    cmd_addr[2] = (addr >> 8) & 0xFF;
    cmd_addr[3] = (addr >> 0) & 0xFF;

    FLASH_CS_LOW();
    if (HAL_SPI_Transmit(&hspi2, cmd_addr, 4, HAL_MAX_DELAY) != HAL_OK) { FLASH_CS_HIGH(); return -1; }
    if (len > 0) {
        if (HAL_SPI_Receive(&hspi2, buf, len, HAL_MAX_DELAY) != HAL_OK) { FLASH_CS_HIGH(); return -1; }
    }
    FLASH_CS_HIGH();
    return 0;
}

static int FLASH_PageProgram(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    if (len == 0 || len > FLASH_PAGE_SIZE) return -1;
    if (FLASH_WriteEnable() != 0) return -1;

    uint8_t cmd_addr[4];
    cmd_addr[0] = CMD_PAGE_PROGRAM;
    cmd_addr[1] = (addr >> 16) & 0xFF;
    cmd_addr[2] = (addr >> 8) & 0xFF;
    cmd_addr[3] = (addr >> 0) & 0xFF;

    DBG_PRINTF("PageProgram addr=0x%06lX len=%lu\n", (unsigned long)addr, (unsigned long)len);

    FLASH_CS_LOW();
    if (HAL_SPI_Transmit(&hspi2, cmd_addr, 4, HAL_MAX_DELAY) != HAL_OK) { FLASH_CS_HIGH(); return -1; }
    if (HAL_SPI_Transmit(&hspi2, (uint8_t*)buf, len, HAL_MAX_DELAY) != HAL_OK) { FLASH_CS_HIGH(); return -1; }
    FLASH_CS_HIGH();

    if (FLASH_WaitUntilReady(FLASH_TIMEOUT_MS) != 0) return -1;
    return 0;
}

static int FLASH_ProgramBuffer(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    uint32_t remaining = len;
    uint32_t cur = addr;
    const uint8_t *p = buf;

    while (remaining > 0) {
        uint32_t page_off = cur & (FLASH_PAGE_SIZE - 1);
        uint32_t space = FLASH_PAGE_SIZE - page_off;
        uint32_t wlen = (remaining < space) ? remaining : space;

        if (FLASH_PageProgram(cur, p, wlen) != 0) return -1;

        cur += wlen;
        p += wlen;
        remaining -= wlen;
    }
    return 0;
}

static int FLASH_SectorErase(uint32_t addr)
{
    if (FLASH_WriteEnable() != 0) return -1;

    uint8_t cmd_addr[4];
    cmd_addr[0] = CMD_SECTOR_ERASE;
    cmd_addr[1] = (addr >> 16) & 0xFF;
    cmd_addr[2] = (addr >> 8) & 0xFF;
    cmd_addr[3] = (addr >> 0) & 0xFF;

    DBG_PRINTF("SectorErase addr=0x%06lX\n", (unsigned long)addr);
    FLASH_CS_LOW();
    if (HAL_SPI_Transmit(&hspi2, cmd_addr, 4, HAL_MAX_DELAY) != HAL_OK) { FLASH_CS_HIGH(); return -1; }
    FLASH_CS_HIGH();

    if (FLASH_WaitUntilReady(FLASH_TIMEOUT_MS) != 0) return -1;
    return 0;
}

/* cache helpers */
static int STORAGE_FlushSectorCache(void)
{
    if (!sector_cache_dirty || sector_cache_addr == 0xFFFFFFFF) return 0;

    DBG_PRINTF("FLUSH sector 0x%08lX\n", (unsigned long)sector_cache_addr);
    if (FLASH_SectorErase(sector_cache_addr) != 0) { DBG_PRINTF("ERR: sector erase\n"); return -1; }
    if (FLASH_ProgramBuffer(sector_cache_addr, sector_cache_buf, FLASH_SECTOR_SIZE) != 0) { DBG_PRINTF("ERR: program buffer\n"); return -1; }

    sector_cache_dirty = false;
    return 0;
}

static int STORAGE_LoadSectorToCache(uint32_t sector_addr)
{
    if (sector_cache_addr == sector_addr) return 0;
    if (sector_cache_dirty) {
        if (STORAGE_FlushSectorCache() != 0) return -1;
    }
    if (FLASH_ReadData(sector_addr, sector_cache_buf, FLASH_SECTOR_SIZE) != 0) return -1;
    sector_cache_addr = sector_addr;
    sector_cache_dirty = false;
    return 0;
}

int STORAGE_Sync(void)
{
    if (STORAGE_FlushSectorCache() != 0) return -1;
    return 0;
}

/* USER CODE END 1 */

/* standard generated functions below (USER CODE blocks preserved) */

int8_t STORAGE_Init_FS(uint8_t lun)
{
  /* USER CODE BEGIN 2 */
  (void)lun;
  FLASH_CS_HIGH();
  sector_cache_addr = 0xFFFFFFFF;
  sector_cache_dirty = false;
  return (USBD_OK);
  /* USER CODE END 2 */
}

int8_t STORAGE_GetCapacity_FS(uint8_t lun, uint32_t *block_num, uint16_t *block_size)
{
  /* USER CODE BEGIN 3 */
  (void)lun;
  *block_num  = STORAGE_BLK_NBR;
  *block_size = STORAGE_BLK_SIZ;
  return (USBD_OK);
  /* USER CODE END 3 */
}

int8_t STORAGE_IsReady_FS(uint8_t lun)
{
  /* USER CODE BEGIN 4 */
  (void)lun;
  if (FLASH_WaitUntilReady(1000) == 0) return (USBD_OK);
  return (USBD_FAIL);
  /* USER CODE END 4 */
}

int8_t STORAGE_IsWriteProtected_FS(uint8_t lun)
{
  /* USER CODE BEGIN 5 */
  (void)lun;
  return (USBD_OK);
  /* USER CODE END 5 */
}

int8_t STORAGE_Read_FS(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len)
{
  /* USER CODE BEGIN 6 */
  (void)lun;
  uint32_t addr = blk_addr * STORAGE_BLK_SIZ;
  uint32_t len = (uint32_t)blk_len * STORAGE_BLK_SIZ;
  uint8_t *p = buf;

  while (len > 0) {
      uint32_t chunk = (len > READ_CHUNK_MAX) ? READ_CHUNK_MAX : len;
      if (FLASH_ReadData(addr, p, chunk) != 0) return USBD_FAIL;
      addr += chunk;
      p += chunk;
      len -= chunk;
  }
  return USBD_OK;
  /* USER CODE END 6 */
}

int8_t STORAGE_Write_FS(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len)
{
  /* USER CODE BEGIN 7 */
  (void)lun;
  uint32_t addr = blk_addr * STORAGE_BLK_SIZ;
  uint32_t total_len = (uint32_t)blk_len * STORAGE_BLK_SIZ;
  uint32_t remaining = total_len;
  uint8_t *pbuf = buf;

  while (remaining > 0) {
      uint32_t sector_index = addr / FLASH_SECTOR_SIZE;
      uint32_t sector_addr = sector_index * FLASH_SECTOR_SIZE;
      uint32_t sector_offset = addr & (FLASH_SECTOR_SIZE - 1);
      uint32_t chunk = FLASH_SECTOR_SIZE - sector_offset;
      if (chunk > remaining) chunk = remaining;

      if (STORAGE_LoadSectorToCache(sector_addr) != 0) { DBG_PRINTF("ERR: load cache\n"); return USBD_FAIL; }

      if (memcmp(&sector_cache_buf[sector_offset], pbuf, chunk) != 0) {
          memcpy(&sector_cache_buf[sector_offset], pbuf, chunk);
          sector_cache_dirty = true;
      }

      /* Force flush after every write for maximum compatibility with Windows formatting */
      if (STORAGE_FlushSectorCache() != 0) { DBG_PRINTF("ERR: flush after write\n"); return USBD_FAIL; }

      addr += chunk;
      pbuf += chunk;
      remaining -= chunk;
  }

  return USBD_OK;
  /* USER CODE END 7 */
}

int8_t STORAGE_GetMaxLun_FS(void)
{
  /* USER CODE BEGIN 8 */
  return (STORAGE_LUN_NBR - 1);
  /* USER CODE END 8 */
}

/* USER CODE BEGIN PRIVATE_FUNCTIONS_IMPLEMENTATION */
/* nothing extra */
/* USER CODE END PRIVATE_FUNCTIONS_IMPLEMENTATION */
