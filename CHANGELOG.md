# Changelog

All notable changes to the `apollo-ps2` project will be documented in this file. This project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]()

## [v1.0.0](https://github.com/bucanero/apollo-ps2/releases/tag/v1.0.0) - 2024-12-07

### Added

* Delete PS2 saves (MemCard and VMC images)
* PS1 MemCard support
  - List, import, export, and delete PS1 saves
  - Import - Supported formats: `.MCS`, `.PSV`, `.PSX`, `.PS1`, `.MCB`, `.PDA`
  - Export - Supported formats: `.MCS`, `.PSV`, `.PSX`
* Manage PS1 Virtual Memory Card images (VMC)
  - Supports `.VMC` and external formats (`.MCR`, `.VMP`, `.BIN`, `.VM1`, `.GME`, `.VGS`, `.SRM`, `.MCD`)
  - List, import, export, and delete PS1 saves inside VMC images
  - Import - Supported formats: `.MCS`, `.PSV`, `.PSX`, `.PS1`, `.MCB`, `.PDA`
  - Export - Supported formats: `.MCS`, `.PSV`, `.PSX`
* Sort saves by Type (Settings)
* Execute `BOOT.ELF` on app exit
  - `mc?:/BOOT/BOOT.ELF`
  - `mc0:/B?DATA-SYSTEM/BOOT.ELF`
  - `mass:/BOOT/BOOT.ELF`

### Fixed

* Fixed GUI color rendering issue

### Misc

* Updated Apollo Patch Engine to v1.1.2
  - Fixed SW Code search bug when bytes are not found
  - Improve code parsing
  - Fix SW Code Type D issue with `CRLF` line breaks
  - Improve SW Code type 3 (add Subtypes `3/7/B/F`)
  - Improve SW code type 4 (add Subtypes `4/5/6/C/D/E`)
  - BSD scripts
    + BSD script command `aes_cbc(key, iv)`
    + Change `compress` and `decompress` command syntax
    + `decompress(offset, wbits)`
    + `compress(offset)`

## [v0.7.0](https://github.com/bucanero/apollo-ps2/releases/tag/v0.7.0) - 2024-02-03

### Added

* Manage PS2 Virtual Memory Card images (VMC)
  * Supports ECC and non-ECC images (`.PS2`, `.VM2`, `.BIN`, `.VMC`)
  * List, import, and export PS2 saves inside VMC images
  * Import - Supported formats: `.PSU`, `.PSV`
  * Export - Supported formats: `.PSU`, `.PSV`

### Fixed

* Fix saving Settings when `mc0:/APOLLO-99PS2/` doesn't exist

### Misc

* Updated Apollo Patch Engine to v0.7.0
  - Add host callbacks (username, system name, wlan mac, psid, account id)
  - Add `murmu3_32`, `jhash`, `jenkins_oaat`, `lookup3_little2` hash functions
  - Add `camellia_ecb` encryption
  - Add MGS5 decryption (PS3/PS4)
  - Add Monster Hunter 2G/3rd PSP decryption
  - Add RGG Studio decryption (PS4)
  - Add Castlevania:LoS checksum
  - Add Dead Rising checksum
  - Add Rockstar checksum
  - Fix SaveWizard Code Type C
  - Fix `right()` on little-endian platforms

## [v0.6.0](https://github.com/bucanero/apollo-ps2/releases/tag/v0.6.0) - 2023-07-29

First public release.

### Added

* Save-game listing (from MC, USB, CD, Host)
* Import saves (in PSU, PSV, MAX, CBS, XPS, SPS formats) to PS2 memcards
* Export and backup memcard saves (to PSU, PSV, CBS, ZIP formats)
* Save-game patching (with SaveWizard / GameGenie codes or BSD scripts)
* Save-data hex editing

### Notes

* Limitations
  * No network features
  * No PS1 save-game support
