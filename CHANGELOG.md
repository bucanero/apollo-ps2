# Changelog

All notable changes to the `apollo-ps2` project will be documented in this file. This project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]()

## [v1.4.0](https://github.com/bucanero/apollo-ps2/releases/tag/v1.4.0) - 2026-11-19

### Added

* Network support
  - Online Database: browse and download saves over the PS2 Network Adapter (or a Slim PS2), on a network with DHCP
  - The network is only started when a download is needed
* Saves Database (formerly Online Database)
  - Offline database: uses the first one found on `cdfs:/APOLLO/` (Apollo disc), `mass:/APOLLO/` or `host:/APOLLO/`, and the Online Database otherwise
  - Import downloaded saves straight to a Memory Card, or copy them to storage
* Apollo disc image (`apollo-ps2.iso`)
  - Boots Apollo, with the offline saves database and all the PS1/PS2 save patches: works without a network connection or USB drive
  - PS2 master disc layout: PS2 logo and master disc sectors (disc ID `APOL-99999`), made with [ps2-master-patcher](https://github.com/bucanero/ps2-master-patcher)
* Built-in save patches
  - The PS1/PS2 save patches are built into the application, no USB drive needed
  - Temporary files are kept in RAM when no USB drive is available
* Python save patches ([MicroPython](https://micropython.org/))
  - For saves that need custom decryption, checksums or edits
  - Python modules are built in, or loaded from `mass:/APOLLO/DATA/python/` or `cdfs:/APOLLO/PATCHES/python/`
* Memory Card dump and restore
  - Dump PS2 Memory Cards to `.PS2` (with ECC) or raw `.VMC` images, and PS1 Memory Cards to `.MCR`
  - Write a PS2 Memory Card image back to a Memory Card
* Apply cheat codes and hex edit save files inside PS1 and PS2 VMC images
* Copy protection: lock or unlock PS2 saves on Memory Cards and PS2 VMC images
  - Select the option, then `Apply Changes`
  - Locked saves are tagged in the save list
* Japanese save titles (Shift-JIS)
  - Drawn with the PS2 BIOS font (`rom0:KROM`), no extra files needed
  - Half-width katakana are shown as full-width
  - Optional font for full coverage (rarer kanji, symbols): `mass:/APOLLO/DATA/FONT.OTF`
* 3D save icons: PS2 save icons are rendered from their 3D model, lit and coloured with the save's `icon.sys` settings

### Fixed

* Copy-protected PS2 saves were missing from the Memory Card save list
* Crash when cancelling the Memory Card selection after changing or deleting a save
* Japanese titles are sorted after the Latin ones (they were listed before `Bulk Save Management`)
* Save import
  - Damaged or truncated saves are rejected (CRC/checksum and size checks) instead of creating a broken save
  - `.MAX`: the last file of some saves was cut off
  - Short or invalid PS1 save files (`.PSV`, `.MCS`, `.PSX`) are rejected instead of being read past their end
  - PS2 saves can't be imported to a PS1 Memory Card

### Misc

* `Apply Changes` is the first option of every save, like on the other Apollo platforms
* Updated Apollo Patch Engine to v3.0.0
* Background music player moved to the EE
* The GitHub Actions workflow builds the Apollo disc image on tags and manual runs

## [v1.0.2](https://github.com/bucanero/apollo-ps2/releases/tag/v1.0.2) - 2024-12-24

### Added

* Support additional PS2 save formats when importing to PS2 VMCs
  - Import to VMC - New supported formats: `.MAX`, `.CBS`, `.XPS`, `.SPS`
* Add support to load an offline save database (from `cdfs:/`, `mass:/`)
  - Note: requires `mass` drive to be available to unzip save files

### Fixed

* Solve issue when exporting PS1 saves from memory cards

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
