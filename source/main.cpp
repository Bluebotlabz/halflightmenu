/*-----------------------------------------------------------------
 Copyright (C) 2005 - 2013
	Michael "Chishm" Chisholm
	Dave "WinterMute" Murphy
	Claudio "sverx"

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

------------------------------------------------------------------*/
#include <nds.h>
#include <stdio.h>
#include <slim.h>
#include <sys/stat.h>
#include <limits.h>

#include <string.h>
#include <unistd.h>

#include "args.h"
#include "file_browse.h"
#include "hbmenu_banner.h"
#include "iconTitle.h"
#include "nds_loader_arm9.h"

#include "ROMList.h"
#include "saveMap.h"

#include "fileCopy.h"
#include "tonccpy.h"

using namespace std;

#define GAMECODE_OFFSET 0x00C
#define CRC_OFFSET 0x15E



/**
 * Fix AP for some games.
 */
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



//---------------------------------------------------------------------------------
void stop (void) {
//---------------------------------------------------------------------------------
	while (1) {
		swiWaitForVBlank();
	}
}

//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
//---------------------------------------------------------------------------------

	// overwrite reboot stub identifier
	// so tapping power on DSi returns to DSi menu
	extern u64 *fake_heap_end;
	*fake_heap_end = 0;

	iconTitleInit();

	// Subscreen as a console
	videoSetModeSub(MODE_0_2D);
	vramSetBankH(VRAM_H_SUB_BG);
	consoleInit(NULL, 0, BgType_Text4bpp, BgSize_T_256x256, 15, 0, false, true);

	if (!fatInitDefault()) {
		iprintf ("fatinitDefault failed!\n");
		stop();
	}

	keysSetRepeat(25,5);

	vector<string> extensionList = argsGetExtensionList();

	chdir("/nds");

	while(1) {

		string filename = browseForFile(extensionList);

		// Construct a command line
		vector<string> argarray; // Stores list of arguments using black magic
		if (!argsFillArray(filename, argarray)) {
			iprintf("Invalid NDS or arg file selected\n");
		} else {
			iprintf("Running %s with %d parameters\n", argarray[0].c_str(), argarray.size());

			char cwd[PATH_MAX];
			getcwd(cwd, PATH_MAX);

			string sCWD(cwd);

			// Check if saves folder exists
			if (access((sCWD + "/saves/").c_str(), F_OK) != 0) {
				mkdir((sCWD + "/saves").c_str(), 0777); // Create saves folder if not exist
			}

			// Check if the save file exists or not
			if (access((sCWD + "/saves/" + filename.substr(0, (filename.length() - 3)) + "sav").c_str(), F_OK) != 0) {
				iprintf("No save file found!\nCreating save file...");
				// Get the gamecode
				FILE *romFile = fopen((sCWD + "/" + filename).c_str(), "rb");
				u32 gameCode;
				if (romFile) {
					fseek(romFile, GAMECODE_OFFSET, SEEK_SET);
					fread(&gameCode, 4, 1, romFile); // Read 1 4-byte element from the romFile (the gameCode)
					fclose(romFile);

					// Write the save file omg
					FILE *saveFile = fopen((sCWD + "/saves/" + filename.substr(0, (filename.length() - 3)) + "sav").c_str(), "wb");

					std::vector<u32>::iterator vecPointer = lower_bound(gameCodes.begin(), gameCodes.end(), gameCode);
					ROMListEntry gameInfo = ROMList[vecPointer - gameCodes.begin()];
					if (saveFile) {
						fseek(saveFile, sramlen[gameInfo.SaveMemType] - 1, SEEK_SET);
						fputc(0, saveFile);
						fclose(saveFile);
					}
				}
			}

			// Write the nds bootstrap config file
			/*FILE *file = fopen("sd:/_nds/nds-bootstrap.ini", "w");
			if (file)
			{
				fputs(
					("[NDS-BOOTSTRAP]\n"
					"DEBUG = 0\n"
					"LOGGING = 0\n"
					"B4DS_MODE = 0\n"
					"ROMREAD_LED = 0\n"
					"DMA_ROMREAD_LED = -1\n"
					"PRECISE_VOLUME_CONTROL = 0\n"
					"SDNAND = 0\n"
					"MACRO_MODE = 0\n"
					"SLEEP_MODE = 1\n"
					"SOUND_FREQ = 0\n"
					"CONSOLE_MODEL = 2\n"
					"HOTKEY = 284\n"
					"USE_ROM_REGION = 1\n"
					"NDS_PATH = " + sCWD + "/" + filename + "\n" + 
					"SAV_PATH = " + sCWD + "/saves/" + filename.substr(0, (filename.length() - 3)) + "sav\n" + // remove the nds part and replace with sav
					"RAM_DRIVE_PATH = sd:/null.img\n"
					"GUI_LANGUAGE = en\n"
					"LANGUAGE = -1\n"
					"REGION = -1\n"
					"DSI_MODE = 1\n"
					"BOOST_CPU = 0\n"
					"BOOST_VRAM = 0\n"
					"CARD_READ_DMA = 1\n"
					"ASYNC_CARD_READ = 0\n"
					"EXTENDED_MEMORY = 0\n"
					"DONOR_SDK_VER = 0\n"
					"PATCH_MPU_REGION = 0\n"
					"PATCH_MPU_SIZE = 0\n"
					"FORCE_SLEEP_PATCH = 0\n").c_str()
				, file);
				fclose(file);
			}
			else
			{
				iprintf("open failed!");
			}*/

			// Get the game AP patch //

			// Get headerCRC + TID
			FILE *romFile = fopen((sCWD + "/" + filename).c_str(), "rb");
			u16 headerCRC;
			char tid[5] = {0}; // 4-character long titleID

			if (romFile) {
				fseek(romFile, GAMECODE_OFFSET, SEEK_SET);
				fread(&tid, 1, 4, romFile);

				fseek(romFile, CRC_OFFSET, SEEK_SET);
				fread(&headerCRC, 2, 1, romFile); // read the 16bit crc of the header

				fclose(romFile);

				iprintf("\nInfo:\n");
				iprintf("TID: %s", tid);
				iprintf("\n");
				iprintf("CRC: %X", headerCRC);
				iprintf("\n");

				// Launch NDS Bootstrap
				vector<const char*> c_args = {
					"sd:/_nds/nds-bootstrap-release.nds",
					strdup((sCWD + "/" + filename).c_str()),
					strdup((sCWD + "/saves/" + filename.substr(0, (filename.length() - 3)) + "sav").c_str()),
					"", "", "", "", "", "", "", "", "", "", "",
					strdup(setApFix((sCWD + "/" + filename).c_str(), tid, headerCRC).c_str()) // argv[14] is AP_FIX_PATH
				};

				int err = runNdsFile(c_args[0], c_args.size(), &c_args[0]);
				iprintf("Start failed. Error %i\n", err);
			} else {
				iprintf("Failed to get ROM data");
			}
		}

		argarray.clear();

		while (1) {
			swiWaitForVBlank();
			scanKeys();
			if (!(keysHeld() & KEY_A)) break;
		}

	}

	return 0;
}
