/*
 * Copyright 2016, The EFIDroid Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#ifndef LIB_BOOT_PLATFORM_H
#define LIB_BOOT_PLATFORM_H

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <limits.h>
#include <stdio.h>

#define LIBBOOT_FMT_UINTN "u"
#define LIBBOOT_FMT_UINT32 "u"
#define LIBBOOT_FMT_ADDR "x"
#define LIBBOOT_FMT_INT "d"

#define LIBBOOT_ASSERT ASSERT
#define LIBBOOT_OFFSETOF(StrucName, Member)  OFFSET_OF(StrucName, Member)

#if !defined(MDEPKG_NDEBUG)
  #define LIBBOOT_DEBUG(fmt, ...)     \
    do {                              \
      if (DebugPrintEnabled ()) {     \
        printf (fmt, ##__VA_ARGS__);  \
      }                               \
    } while (FALSE)
#else
  #define LIBBOOT_DEBUG(Expression)
#endif

#define LOGV(fmt, ...) LIBBOOT_DEBUG(fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) printf(fmt, ##__VA_ARGS__)
#define LOGI(fmt, ...) LIBBOOT_DEBUG(fmt, ##__VA_ARGS__)

typedef UINTN  boot_uintn_t;
typedef INTN   boot_intn_t;
typedef UINT8  boot_uint8_t;
typedef UINT16 boot_uint16_t;
typedef UINT32 boot_uint32_t;
typedef UINT64 boot_uint64_t;

#endif // LIB_BOOT_PLATFORM_H
