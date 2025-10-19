/*
 * usbd_storage_alias.c
 *
 *  Created on: Oct 4, 2025
 *      Author: SNYDE
 */

/* prosty alias - tylko na potrzeby testu/linku.
   Usuń ten plik, kiedy oryginalny usbd_storage_if.c jest poprawnie dodany do projektu. */
#include "usbd_storage_if.h"

/* Deklaracje funkcji, które usbd_storage_if.c zwykle dostarcza.
   Jeśli masz oryginalny plik, to ten plik spowoduje "multiple definition" — wtedy usuń go. */
extern int8_t STORAGE_Init_FS(uint8_t lun);
extern int8_t STORAGE_GetCapacity_FS(uint8_t lun, uint32_t *block_num, uint16_t *block_size);
extern int8_t STORAGE_IsReady_FS(uint8_t lun);
extern int8_t STORAGE_IsWriteProtected_FS(uint8_t lun);
extern int8_t STORAGE_Read_FS(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len);
extern int8_t STORAGE_Write_FS(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len);
extern int8_t STORAGE_GetMaxLun_FS(void);

USBD_StorageTypeDef USBD_Storage_Interface_fops_FS =
{
  STORAGE_Init_FS,
  STORAGE_GetCapacity_FS,
  STORAGE_IsReady_FS,
  STORAGE_IsWriteProtected_FS,
  STORAGE_Read_FS,
  STORAGE_Write_FS,
  STORAGE_GetMaxLun_FS,
  (int8_t *)0 /* inquiry data - niepotrzebne do testu linku */
};
