# GPL v3 Compliance Summary

This document confirms compliance with the GNU General Public License v3.0 for this derivative work.

## ✅ GPL v3 Requirements Met

### 1. License Text
- [x] Full GPL v3 license text included in `LICENSE` file
- [x] File located at repository root
- [x] Unmodified GPL v3 text (675 lines)

### 2. Copyright Notices
- [x] Original author attributed: Christopher Hanger (changer65535)
- [x] Attribution in main source file (`src/main.cpp`)
- [x] Attribution in README.md
- [x] Attribution in FORK_CHANGES.md
- [x] GPL v3 allows modifications without claiming copyright

### 3. Source Code Disclosure
- [x] All source code publicly available on GitHub
- [x] Repository: https://github.com/tydanielson/ModeWifi
- [x] No object code distribution without source
- [x] Clear instructions for building in README.md

### 4. Modification Documentation
- [x] FORK_CHANGES.md documents all modifications
- [x] Changes clearly marked as different from original
- [x] Attribution to original author maintained
- [x] Git commit history preserves modification timeline

### 5. License Notices
- [x] GPL v3 license clearly stated in README.md
- [x] Link to full license text in documentation
- [x] Statement that this is free software under GPL v3
- [x] Notice that derivative work is also GPL v3

### 6. Original Work Preservation
- [x] Original files backed up: `src/main.cpp.old`, `src/cm.cpp.old`, `src/cm.h.old`
- [x] Original README preserved: `README_original.md`
- [x] Original LICENSE file retained (GPL v3)
- [x] Attribution to original author in multiple places

## 📋 Copyleft Obligations

As a derivative work under GPL v3:

1. **This project MUST remain GPL v3** - Cannot be relicensed to MIT, Apache, or proprietary
2. **Source code MUST be available** - Anyone receiving binaries must also receive source
3. **Modifications MUST be documented** - Changes must be marked with dates
4. **License MUST be included** - GPL v3 text must accompany all distributions

## 🔍 Key Compliance Points

### Original Author
- **Name:** Christopher Hanger
- **GitHub:** changer65535
- **Original Repo:** https://github.com/changer65535/ModeWifi
- **Original License:** GNU GPL v3.0

### This Fork
- **Fork Repo:** https://github.com/tydanielson/ModeWifi
- **License:** GNU GPL v3.0 (required by copyleft)
- **Date Created:** December 2025
- **Attribution:** Based on original work by Christopher Hanger

### Major Changes
- Migration from MCP2515 SPI to ESP32 native CAN (TWAI)
- Hardware change to SN65HVD230 3.3V transceiver
- Complete code refactoring to modular architecture
- Dual-core FreeRTOS implementation
- Mobile-optimized web interface
- PlatformIO build system

## ✅ GPL v3 License Header Template

All significant source files include this header:

```cpp
/*
 * ModeWifi - ESP32 CAN Bus Interface for Storyteller Overland Sprinter
 * 
 * Based on original work by Christopher Hanger (changer65535)
 * https://github.com/changer65535/ModeWifi
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
```

## 📄 Distribution Requirements

When distributing this software:

1. ✅ Include full source code (or offer access)
2. ✅ Include LICENSE file with GPL v3 text
3. ✅ Preserve copyright notices
4. ✅ Include installation instructions
5. ✅ Document any modifications made
6. ✅ License derivative works under GPL v3

## 🔗 References

- **GPL v3 Full Text:** https://www.gnu.org/licenses/gpl-3.0.html
- **GPL FAQ:** https://www.gnu.org/licenses/gpl-faq.html
- **Why GPL:** https://www.gnu.org/licenses/why-not-lgpl.html

## ✍️ Compliance Certification

This project has been reviewed for GPL v3 compliance and meets all requirements:

- [x] License text present
- [x] Copyright notices correct
- [x] Source code disclosed
- [x] Modifications documented
- [x] Attribution preserved
- [x] Copyleft maintained

**Date:** December 6, 2025  
**Status:** ✅ **COMPLIANT**
