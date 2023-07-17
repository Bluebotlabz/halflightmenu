#include <string>

#include "fileCopy.h"
#include "tonccpy.h"

#include "twl_util.h"


std::string setApFix(const char *filename, const char* gameTid, u16 headerCRC) {
	bool ipsFound = false;
	bool cheatVer = true;
	char ipsPath[256];
	char ipsPath2[256];

	iprintf("Checking for bin APFix (1)");
	if (!ipsFound) {
		snprintf(ipsPath, sizeof(ipsPath), "sd:/_nds/TWiLightMenu/extras/apfix/cht/%s.bin", filename);
		ipsFound = (access(ipsPath, F_OK) == 0);
	}

	iprintf("Checking for bin APFix (2)");
	if (!ipsFound) {
		snprintf(ipsPath, sizeof(ipsPath), "sd:/_nds/TWiLightMenu/extras/apfix/cht/%s-%X.bin", gameTid, headerCRC);
		ipsFound = (access(ipsPath, F_OK) == 0);
	}

	iprintf("Checking for ips APFix (1)");
	if (!ipsFound) {
		snprintf(ipsPath, sizeof(ipsPath), "sd:/_nds/TWiLightMenu/extras/apfix/%s.ips", filename);
		ipsFound = (access(ipsPath, F_OK) == 0);
		if (ipsFound) {
			cheatVer = false;
		}
	}

	iprintf("Checking for ips APFix (2)");
	if (!ipsFound) {
		snprintf(ipsPath, sizeof(ipsPath), "sd:/_nds/TWiLightMenu/extras/apfix/%s-%X.ips", gameTid, headerCRC);
		ipsFound = (access(ipsPath, F_OK) == 0);
		if (ipsFound) {
			cheatVer = false;
		}
	}

	if (ipsFound) {
		/*if (false && true) {
			mkdir("fat:/_nds", 0777);
			mkdir("fat:/_nds/nds-bootstrap", 0777);
			fcopy(ipsPath, cheatVer ? "fat:/_nds/nds-bootstrap/apFixCheat.bin" : "fat:/_nds/nds-bootstrap/apFix.ips");
			return cheatVer ? "fat:/_nds/nds-bootstrap/apFixCheat.bin" : "fat:/_nds/nds-bootstrap/apFix.ips";
		}*/
		return ipsPath;
	} else {
		iprintf("Checking for pck APFix");
		FILE *file = fopen("sd:/_nds/TWiLightMenu/extras/apfix.pck", "rb");
		if (file) {
			char buf[5] = {0};
			fread(buf, 1, 4, file);
			if (strcmp(buf, ".PCK") != 0) // Invalid file
				return "";

			u32 fileCount;
			fread(&fileCount, 1, sizeof(fileCount), file);

			u32 offset = 0, size = 0;

			// Try binary search for the game
			int left = 0;
			int right = fileCount;

			while (left <= right) {
				int mid = left + ((right - left) / 2);
				fseek(file, 16 + mid * 16, SEEK_SET);
				fread(buf, 1, 4, file);
				int cmp = strcmp(buf, gameTid);
				if (cmp == 0) { // TID matches, check CRC
					u16 crc;
					fread(&crc, 1, sizeof(crc), file);

					if (crc == headerCRC) { // CRC matches
						fread(&offset, 1, sizeof(offset), file);
						fread(&size, 1, sizeof(size), file);
						cheatVer = fgetc(file) & 1;
						break;
					} else if (crc < headerCRC) {
						left = mid + 1;
					} else {
						right = mid - 1;
					}
				} else if (cmp < 0) {
					left = mid + 1;
				} else {
					right = mid - 1;
				}
			}

			if (offset > 0 && size > 0) {
				fseek(file, offset, SEEK_SET);
				u8 *buffer = new u8[size];
				fread(buffer, 1, size, file);

				snprintf(ipsPath, sizeof(ipsPath), "sd:/_nds/nds-bootstrap/apFix%s", cheatVer ? "Cheat.bin" : ".ips");
				snprintf(ipsPath2, sizeof(ipsPath2), "sd:/_nds/nds-bootstrap/apFix%s", cheatVer ? ".ips" : "Cheat.bin");
				if (access(ipsPath2, F_OK) == 0) {
					remove(ipsPath2); // Delete leftover AP-fix file of opposite format
				}
				FILE *out = fopen(ipsPath, "wb");
				if (out) {
					fwrite(buffer, 1, size, out);
					fclose(out);
				}
				delete[] buffer;
				fclose(file);
				return ipsPath;
			}

			fclose(file);
		}
	}

	return "";
}