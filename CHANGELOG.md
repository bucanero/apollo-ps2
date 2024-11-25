# Changelog

All notable changes to the `apollo-ps2` project will be documented in this file. This project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]()

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
