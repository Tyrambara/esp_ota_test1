# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Users/Tyram/esp/v5.2.2/esp-idf/components/bootloader/subproject"
  "D:/YandexDisk/4Proging/ESP32/esp_ota2/build/bootloader"
  "D:/YandexDisk/4Proging/ESP32/esp_ota2/build/bootloader-prefix"
  "D:/YandexDisk/4Proging/ESP32/esp_ota2/build/bootloader-prefix/tmp"
  "D:/YandexDisk/4Proging/ESP32/esp_ota2/build/bootloader-prefix/src/bootloader-stamp"
  "D:/YandexDisk/4Proging/ESP32/esp_ota2/build/bootloader-prefix/src"
  "D:/YandexDisk/4Proging/ESP32/esp_ota2/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/YandexDisk/4Proging/ESP32/esp_ota2/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "D:/YandexDisk/4Proging/ESP32/esp_ota2/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
