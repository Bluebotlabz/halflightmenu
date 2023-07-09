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


using namespace std;

#define GAMECODE_OFFSET 0x00C

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
						for (int i=0; i < sramlen[gameInfo.SaveMemType]; i++) {
							fputs("\x00", saveFile);
						}

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

			// Launch NDS Bootstrap
			vector<const char*> c_args = {
				"sd:/_nds/nds-bootstrap-release.nds",
				strdup((sCWD + "/" + filename).c_str()),
				strdup((sCWD + "/saves/" + filename.substr(0, (filename.length() - 3)) + "sav").c_str())
			};

			int err = runNdsFile(c_args[0], c_args.size(), &c_args[0]);
			iprintf("Start failed. Error %i\n", err);
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
