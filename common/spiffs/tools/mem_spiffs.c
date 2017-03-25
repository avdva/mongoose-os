/*
 * Copyright (c) 2014-2017 Cesanta Software Limited
 * All rights reserved
 */

#include "mem_spiffs.h"

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include "/usr/include/dirent.h"

#include "common/cs_crc32.h"

#include <spiffs.h>

#define STRINGIFY(a) STRINGIFY2(a)
#define STRINGIFY2(a) #a

spiffs fs;
u8_t spiffs_work_buf[LOG_PAGE_SIZE * 2];
u8_t spiffs_fds[32 * 4];

char *image; /* in memory flash image */
size_t image_size;
bool log_reads = false, log_writes = false, log_erases = false;
int opr, opw, ope;
int wfail = -1;

s32_t mem_spiffs_read(spiffs *fs, u32_t addr, u32_t size, u8_t *dst) {
  if (log_reads) {
    fprintf(stderr, "R   #%04d %d @ %d\n", opr, (int) size, (int) addr);
  }
  memcpy(dst, image + addr, size);
  opr++;
  return SPIFFS_OK;
}

s32_t mem_spiffs_write(spiffs *fs, u32_t addr, u32_t size, u8_t *src) {
  if (log_writes) {
    fprintf(stderr, " W  #%04d %d @ %d\n", opw, (int) size, (int) addr);
  }
  if (opw == wfail) {
    fprintf(stderr, "=== BOOM!\n");
    mem_spiffs_dump("boom.bin");
    exit(0);
  }
  memcpy(image + addr, src, size);
  opw++;
  return SPIFFS_OK;
}

s32_t mem_spiffs_erase(spiffs *fs, u32_t addr, u32_t size) {
  if (log_erases) {
    fprintf(stderr, "  E #%04d %d @ %d\n", ope, (int) size, (int) addr);
  }
  memset(image + addr, 0xff, size);
  ope++;
  return SPIFFS_OK;
}

int mem_spiffs_mount(void) {
  spiffs_config cfg;

  cfg.phys_size = image_size;
  cfg.phys_addr = 0;

  cfg.phys_erase_block = FLASH_BLOCK_SIZE;
  cfg.log_block_size = FLASH_BLOCK_SIZE;
  cfg.log_page_size = LOG_PAGE_SIZE;

  cfg.hal_read_f = mem_spiffs_read;
  cfg.hal_write_f = mem_spiffs_write;
  cfg.hal_erase_f = mem_spiffs_erase;

  return SPIFFS_mount(&fs, &cfg, spiffs_work_buf, spiffs_fds,
                      sizeof(spiffs_fds), 0, 0, 0);
}

bool mem_spiffs_dump(const char *fname) {
  FILE *out = fopen(fname, "w");
  if (out == NULL) {
    fprintf(stderr, "failed to open %s for writing\n", fname);
    return false;
  }
  if (fwrite(image, image_size, 1, out) != 1) {
    fprintf(stderr, "write failed\n");
    return false;
  }
  fclose(out);
  fprintf(stderr, "== dumped fs to %s\n", fname);
  return true;
}

u32_t list_files(void) {
  spiffs_DIR d;
  struct spiffs_dirent de;
  if (SPIFFS_opendir(&fs, ".", &d) == NULL) {
    fprintf(stderr, "opendir error: %d\n", SPIFFS_errno(&fs));
    return 0;
  }

  fprintf(stderr, "\n-- files:\n");
  uint32_t overall_size = 0, overall_crc32 = 0;
  while (SPIFFS_readdir(&d, &de) != NULL) {
    char *buf = NULL;
    spiffs_file in;

    in = SPIFFS_open_by_dirent(&fs, &de, SPIFFS_RDONLY, 0);
    if (in < 0) {
      fprintf(stderr, "cannot open spiffs file %s, err: %d\n", de.name,
              SPIFFS_errno(&fs));
      return 0;
    }

    buf = malloc(de.size);
    if (SPIFFS_read(&fs, in, buf, de.size) != de.size) {
      fprintf(stderr, "cannot read %s, err: %d\n", de.name, SPIFFS_errno(&fs));
      return 0;
    }
    uint32_t crc32 = cs_crc32(0, buf, de.size);
    char fmt[32];
    sprintf(fmt, "%%-%ds %%5d 0x%%08x", SPIFFS_OBJ_NAME_LEN);
    fprintf(stderr, fmt, de.name, (int) de.size, crc32);
#if SPIFFS_OBJ_META_LEN > 0
    fprintf(stderr, " meta:");
    for (int i = 0; i < SPIFFS_OBJ_META_LEN; i++) {
      fprintf(stderr, " %02x", de.meta[i]);
    }
#endif
    fprintf(stderr, "\n");
    free(buf);
    SPIFFS_close(&fs, in);
    overall_size += de.size;
    overall_crc32 ^= crc32;
  }

  SPIFFS_closedir(&d);
  fprintf(stderr, "-- overall size: %u, crc32: 0x%08x\n", overall_size,
          overall_crc32);
  return overall_crc32;
}
