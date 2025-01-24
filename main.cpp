
#include <iostream>
#include <filesystem>
#include <cstring>
#include <cstdio>

using namespace std::filesystem;

static const path expected_files[] = {
	"data/astarisborn.rsc",
	"data/contents.rsc",
	"data/dohrayme.rsc",
	"data/dohrayme2.rsc",
	"data/endgame_en.rsc",
	"data/Ergonomics.bik",
	"data/Ergonomics_short.bik",
	"data/games.rsc",
	"data/games_en.rsc",
	"data/intro.rsc",
	"data/lescont.rsc",
	"data/lesson.rsc",
	"data/lesson_en.rsc",
	"data/logo.bik",
	"data/M01_IntroMovie.bik",
	"data/M02_Round1Intro.bik",
	"data/M03_Round2Intro.bik",
	"data/M04_Round3Intro.bik",
	"data/M05_Round4Intro.bik",
	"data/M06_Round5Intro.bik",
	"data/M07_Final1Intro.bik",
	"data/M08_Final1Intro.bik",
	"data/M09_Final1Intro.bik",
	"data/M10_AdvancedLevelsIntro.bik",
	"data/M10_IntroToAdvanced.bik",
	"data/M11_FinaleMovie.bik",
	"data/main.rsc",
	"data/main_en.rsc",
	"data/mazegame.rsc",
	"data/practice.rsc",
	"data/practicearea.rsc",
	"data/practicearea2.rsc",
	"data/racegame.rsc",
	"data/spacgame.rsc",
	"HD/LAUNCHER/installed.xml",
	"HD/LAUNCHER/launch.xml",
	"HD/LAUNCHER/launcherData.xml",
	"HD/LAUNCHER/mps/salstart.mps",
	"HD/LAUNCHER/salrsc/SalCommon.rsc",
	"HD/LAUNCHER/salrsc/SalSignIn.rsc",
	"HD/LAUNCHER/salrsc/SalStartup.rsc",
	"HD/LAUNCHER/salrsc/TLCSignin.xml",
	"HD/LAUNCHER/win/TLCLauncher.exe",
	"HD/LAUNCHER/winrsc/salstartup.xml",
	"HD/SPT/binkw32.dll",
	"HD/SPT/mss32.dll",
	"HD/SPT/ReadMe.txt",
	"HD/SPT/SPT.exe",
	"HD/SPT/data/menuexres.rsc",
	"HD/SPT/scripts/creaturelab.mps",
	"HD/SPT/scripts/credits.mps",
	"HD/SPT/scripts/dohrayme.mps",
	"HD/SPT/scripts/ergonomics.mps",
	"HD/SPT/scripts/games.mps",
	"HD/SPT/scripts/hub.mps",
	"HD/SPT/scripts/intro.mps",
	"HD/SPT/scripts/lesson.mps",
	"HD/SPT/scripts/mazegame.mps",
	"HD/SPT/scripts/practice.mps",
	"HD/SPT/scripts/racegame.mps",
	"HD/SPT/scripts/spacgame.mps",
};

static char *strstr2(char *str1, char *str2) {
	char *s = strstr(str1, str2);
	return s ? s + strlen(str2) : nullptr;
}

static bool check_disk_files(const path &disk) {
	bool files_exist = true;
	for (const auto &file : expected_files) {
		path full_path = disk / file;
		if (!exists(full_path) || !is_regular_file(full_path)) {
			files_exist = false;
			std::cout << "Unable to find: " << file << '\n';
		}
	}
	return files_exist;
}

static bool copy_files(const path &disk, const path &out) {
	std::error_code error;

	#define CHECK(exp)                            \
	    exp;                                      \
	    if (error) {                              \
	        std::cout << "Error copying fileszn"; \
	        return false;                         \
	    }                                         \

	CHECK(copy(disk / "HD" / "SPT", out, copy_options::recursive, error))
	CHECK(create_directories(out / "save" / "TLC Launcher", error))

	const path disk_launcher = disk / "HD" / "LAUNCHER";
	CHECK(copy(disk_launcher / "win" / "TLCLauncher.exe", out, copy_options::overwrite_existing, error))
	CHECK(copy(disk_launcher / "mps", out / "mps", error))
	CHECK(copy(disk_launcher / "salrsc", out / "salrsc", error))

	CHECK(copy(disk / "data", out / "data", error))

	#undef CHECK

	return true;
}

static bool patch_game(const path &out) {
	FILE *file = fopen((out / "SPT.exe").string().c_str(), "rb+");
	if (!file) {
		std::cout << "Unable to open SPT.exe\n";
		return false;
	}

	// Change save directory.
	const int nop = 0x90;
	fseek(file, 0x8044D, SEEK_SET);
	for (int i = 0; i < 14; ++i)
		fwrite(&nop, 1, 1, file);
	fseek(file, 0xF83D8, SEEK_SET);
	fwrite(".\\", 1, 3, file);
	fseek(file, 0xF836C, SEEK_SET);
	fwrite("save", 1, 5, file);

	fclose(file);

	return true;
}

static bool patch_launcher(const path &out) {
	FILE *file = fopen((out / "TLCLauncher.exe").string().c_str(), "rb+");
	if (!file) {
		std::cout << "Unable to open TLCLauncher.exe\n";
		return false;
	}

	// Move save directory outside of ProgramData.
	const int nop = 0x90;
	fseek(file, 0x683E3, SEEK_SET);
	for (int i = 0; i < 5; ++i)
		fwrite(&nop, 1, 1, file);

	// Close launcher when the game opens.
	unsigned char close_ops[] = { 0x6A, 0x00, 0xFF, 0x15, 0xE4, 0x1C };
	fseek(file, 0x1B641, SEEK_SET);
	fwrite(close_ops, 1, sizeof(close_ops), file);

	// Remove unsupported os popup.
	fseek(file, 0x5BF45, SEEK_SET);
	for (int i = 0; i < 10; ++i)
		fwrite(&nop, 1, 1, file);

	fclose(file);

	return true;
}

static bool configure(const path &disk, const path &out) {
	FILE *file = fopen((out / "TLC.ini").string().c_str(), "wb");
	if (!file) {
		std::cout << "Could not open TLC.ini\n";
		return false;
	}
	char tlc_ini[] = "[SpongeBob Typing]\nCDDrive=.\\";
	fwrite(tlc_ini, 1, sizeof(tlc_ini) - 1, file);
	fclose(file);

	file = fopen((disk / "HD" / "LAUNCHER" / "winrsc" / "salstartup.xml").string().c_str(), "rb");
	if (!file) {
		std::cout << "Could not open salstartup.xml\n";
		return false;
	}
	fseek(file, 0, SEEK_END);
	int file_size = (int)ftell(file);
	char *buffer = new char[file_size + 1];
	rewind(file);
	fread(buffer, 1, file_size, file);
	fclose(file);

	file = fopen((out / "salrsc" / "salstartup.xml").string().c_str(), "wb");
	if (!file) {
		std::cout << "Could not write to salstartup.xml\n";
		delete[] buffer;
		return false;
	}
	char *it = buffer;
	
	char *next = strstr2(it, "<userPath>");
	fwrite(it, 1, next - it, file);
	it = next;

	const char user_path[] = "\".\\save\\TLC Launcher\"";
	fwrite(user_path, 1, sizeof(user_path) - 1, file);

	next = strstr(it, "<cdCheck>");
	fwrite(it, 1, next - it, file);
	it = next + 27;

	next = strstr2(it, "<target>");
	fwrite(it, 1, next - it, file);
	it = next;

	const char target[] = ".\\SPT.exe";
	fwrite(target, 1, sizeof(target) - 1, file);

	fwrite(it, 1, buffer + file_size - it, file);
	fclose(file);

	delete[] buffer;

	return true;
}

static void install(int argc, char **argv) {
	path disk = argv[0];
	path out  = argv[1];

	if (!is_directory(disk)) {
		std::cout << "<disk> must be a directory";
		return;
	}
	if (!exists(out)) {
		create_directory(out);
	}
	else if (!is_directory(out)) {
		std::cout << "<out> must be a directory";
		return;
	}
	if (!is_empty(out)) {
		std::cout << "<out> must be empty";
		return;
	}

	#define CHECK(exp) \
	    if (!exp)      \
	        return;    \

	CHECK(check_disk_files(disk))
	CHECK(copy_files(disk, out))
	CHECK(patch_game(out))
	CHECK(patch_launcher(out))
	CHECK(configure(disk, out))

	#undef CHECK
}

int main(int argc, char **argv) {
	if (argc < 3) {
		std::cout << "Usage: spt_intall <disk> <out>\n";
		return 0;
	}

	install(argc - 1, argv + 1);
}
