#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace std;
using uchar = unsigned char;
using ushort = unsigned short;
using uint = unsigned;
using i64 = int64_t;
using __int64 = int64_t;
using DWORD = uint32_t;
using HANDLE = void*;
using PLAYER_COMMAND = char;
using DUNGEON = char;
using PLAYER_SUBLASS = char;
using PLAYER_SPECIALIZATION = char;
using GAME_MODE = char;
using QUENCH_AFFIX = uchar;
using TraitId = uchar;
struct StructVersion { int version, size; };
struct Item;
struct ItemGet;
constexpr int SocketMax_6 = 6, EnchantMax_5 = 5, PERKS_MAX_800 = 800;
constexpr int SHA1_BLOCK_SIZE = 64, SHA1_DIGEST_SIZE = 20, VERSION = 0;
constexpr uint SaveSignature = 0x48454c34;
constexpr int MAX_PATH = 260, MaxSaveSlots = 101;
constexpr DWORD INVALID_FILE_ATTRIBUTES = 0xffffffffu;
constexpr DWORD ERROR_FILE_NOT_FOUND = 2, ERROR_PATH_NOT_FOUND = 3, ERROR_ACCESS_DENIED = 5;
#define __fastcall
#define __stdcall
#define memzero(value) memset(&(value), 0, sizeof(value))
#define THE_HELL_VERSION_STRING "v0.774.1\0"
#pragma pack(push, 1)
#include "save_layout.inc"
#pragma pack(pop)
static_assert(sizeof(PlayerInfo_v26) == 11817);

uint TestRandState;
void TestSrand(uint seed) { TestRandState = seed; }
int TestRand() { TestRandState = TestRandState * 214013u + 2531011u; return (TestRandState >> 16) & 0x7fff; }
uint GetTickCount() { return 0; }
void ShaInitialize(int);
void ShaDestroy();
void ShaGetLastHash(int, uchar*);
void ShaHash(int, uchar*, uchar*);
void UpdateHash(ShaInfo&, uchar*, int);
void TransformHash(ShaInfo&);
void InitializeKeys(bool, char*);
[[noreturn]] void TerminateWithError(const char*, ...) { abort(); }
constexpr size_t CalcEncodeDstBytes(size_t size) { return ((size + 63) & ~size_t(63)) + sizeof(AppendRec); }
#define srand TestSrand
#define rand TestRand
#include "save_crypto.inc"
#undef rand
#undef srand

char CryptKey[9] = "xrgyrkj1";
char* SaveFileHero() { static char name[] = "mphero"; return name; }
vector<uchar> Encoded;
enum class ReadMode { Success, Partial, Failure };
ReadMode Mode = ReadMode::Success;
int Allocations = 0, Closed = 0;
void* AllocMem(uint size) { ++Allocations; return malloc(size); }
template<class T> void FreeMemZero(T*& pointer) { if(pointer) { --Allocations; free(pointer); pointer = nullptr; } }
bool SFileOpenFileEx(HANDLE, char*, int, HANDLE* handle) { *handle = reinterpret_cast<HANDLE>(1); return true; }
DWORD SFileGetFileSize(HANDLE, void*) { return Encoded.size(); }
bool SFileReadFile(HANDLE, void* data, DWORD size, DWORD* read, int)
{
	memcpy(data, Encoded.data(), size);
	*read = Mode == ReadMode::Partial ? size - 1 : size;
	return Mode != ReadMode::Failure;
}
void SFileCloseFile(HANDLE) { ++Closed; }

using CharName = char[32];
CharName CharNameSlots[MaxSaveSlots]{};
struct CharSaveInfo
{
	char Name[32];
	int Class{}, SubClass{}, Specialization{}, GameMode{}, GameChanger{}, Traits{};
};
struct Player { char playerName[32]; } Players[1];
struct AdditionalInfo { int StashTabsPurchased, StashCurrentTab; char creation_version[16]; } AdditionalStateData;
filesystem::path SaveRoot;
int DeniedSlot = -1, OpenedSlot = -1;
DWORD LastError;
bool UberDiabloKilled = false;
ofstream Archive;
void GetSaveArchivePath(char (&path)[MAX_PATH], int, int slot, int)
{
	strcpy(path, (SaveRoot / ("th4mp_" + to_string(slot) + ".msv")).string().c_str());
}
DWORD GetFileAttributesA(const char* path)
{
	if( DeniedSlot >= 0 && filesystem::path(path).filename() == "th4mp_" + to_string(DeniedSlot) + ".msv" ){
		LastError = ERROR_ACCESS_DENIED;
		return INVALID_FILE_ATTRIBUTES;
	}
	if( filesystem::exists(path) ) return 0;
	LastError = ERROR_FILE_NOT_FOUND;
	return INVALID_FILE_ATTRIBUTES;
}
DWORD GetLastError() { return LastError; }
int GetSlotByHeroName(const char* name)
{
	for( int i = 0; i < MaxSaveSlots - 1; ++i ) if( strcmp(CharNameSlots[i], name) == 0 ) return i;
	return MaxSaveSlots - 1;
}
bool OpenArchiveWrite(bool, int slot)
{
	OpenedSlot = slot;
	Archive.open(SaveRoot / ("th4mp_" + to_string(slot) + ".msv"), ios::binary | ios::trunc);
	return bool(Archive);
}
void CloseArchiveWrite(bool, int) { Archive.close(); }
void GetPlayerFileNames() {}
void MpqDeleteFiles(void (*)()) {}
void CreatePlayer(int, int, int, int, int, int, int) {}
void SavePlayerInfo(LastPlayerInfo* info, int) { memset(info, 0, sizeof(*info)); strcpy(info->CharName, Players[0].playerName); }
void SaveCharacter(LastPlayerInfo* info) { Archive.write(info->CharName, strlen(info->CharName)); }
void WriteAdditionalHeroDataToSave() {}
void WriteHotkeysDataToSave() {}
void InitCharData(Player&, CharSaveInfo&, bool) {}
#include "save_under_test.inc"

vector<uchar> Record(uint version, size_t size)
{
	vector<uchar> plain(size);
	PlayerInfo header{SaveSignature, version};
	memcpy(plain.data(), &header, min(size, sizeof(header)));
	return plain;
}

void SetEncoded(const vector<uchar>& plain)
{
	Encoded.assign(CalcEncodeDstBytes(plain.size()), 0);
	copy(plain.begin(), plain.end(), Encoded.begin());
	EncodeFile(Encoded.data(), plain.size(), Encoded.size(), CryptKey);
}

void TestReader()
{
	const pair<uint, size_t> supported[] = {
		{18, sizeof(PlayerInfo_v18)}, {21, sizeof(PlayerInfo_v21)},
		{22, sizeof(PlayerInfo_v22)}, {26, 11798}, {27, 11798},
		{27, 11798 + POWERUP_COUNT}, {28, sizeof(PlayerInfo_v26)},
		{CurSaveVersion, sizeof(PlayerInfo_v26)},
	};
	for( auto [version, size] : supported ){
		auto plain = Record(version, size);
		SetEncoded(plain);
		auto* result = ReadPlayerInfo(nullptr);
		assert(result && memcmp(result, plain.data(), plain.size()) == 0);
		FreeMemZero(result);
	}
	for( const auto& plain : {Record(CurSaveVersion + 1, sizeof(PlayerInfo_v26)), Record(30, 11798), Record(26, 11817), Record(0, sizeof(PlayerInfo_v26))} ){
		SetEncoded(plain);
		assert(ReadPlayerInfo(nullptr) == nullptr && Allocations == 0);
	}
	SetEncoded(Record(CurSaveVersion, sizeof(PlayerInfo_v26)));
	Encoded[80] ^= 0x40;
	assert(ReadPlayerInfo(nullptr) == nullptr && Allocations == 0);
	SetEncoded(Record(CurSaveVersion, sizeof(PlayerInfo_v26)));
	for( auto mode : {ReadMode::Partial, ReadMode::Failure} ){
		Mode = mode;
		assert(ReadPlayerInfo(nullptr) == nullptr && Allocations == 0);
	}
	Mode = ReadMode::Success;
	cout << "PASS real codec/reader: v30 and legacy saves accepted; newer versions, wrong sizes, bad checksums and incomplete reads rejected\n";
}

string ReadFile(const filesystem::path& path)
{
	ifstream file(path, ios::binary);
	return string(istreambuf_iterator<char>(file), {});
}

void ResetDirectory()
{
	filesystem::remove_all(SaveRoot);
	filesystem::create_directories(SaveRoot);
	memzero(CharNameSlots);
	DeniedSlot = OpenedSlot = -1;
}

void Existing(int slot)
{
	ofstream file(SaveRoot / ("th4mp_" + to_string(slot) + ".msv"), ios::binary);
	file << "EXISTING_UNREADABLE_V30_SLOT_" << slot;
}

void TestCreation()
{
	CharSaveInfo hero{};
	strcpy(hero.Name, "ASD");
	ResetDirectory();
	for( int i = 0; i < 3; ++i ) Existing(i);
	assert(UiCreateHero(&hero) && OpenedSlot == 3);
	for( int i = 0; i < 3; ++i ) assert(ReadFile(SaveRoot / ("th4mp_" + to_string(i) + ".msv")) == "EXISTING_UNREADABLE_V30_SLOT_" + to_string(i));
	assert(ReadFile(SaveRoot / "th4mp_3.msv") == "ASD");
	ResetDirectory();
	for( int i = 0; i < MaxSaveSlots - 1; ++i ) Existing(i);
	assert(!UiCreateHero(&hero) && OpenedSlot == -1);
	for( int i = 0; i < MaxSaveSlots - 1; ++i ) assert(ReadFile(SaveRoot / ("th4mp_" + to_string(i) + ".msv")) == "EXISTING_UNREADABLE_V30_SLOT_" + to_string(i));
	ResetDirectory();
	DeniedSlot = 0;
	assert(UiCreateHero(&hero) && OpenedSlot == 1);
	assert(!filesystem::exists(SaveRoot / "th4mp_0.msv"));
	cout << "PASS hero creation: existing unreadable saves preserved, full slots refused, access failures reserved\n";
}

int main(int argc, char** argv)
{
	assert(argc == 2 || argc == 3);
	SaveRoot = argv[1];
	TestReader();
	TestCreation();
	if( argc == 3 ){
		ifstream file(argv[2], ios::binary);
		assert(file);
		Encoded = vector<uchar>(istreambuf_iterator<char>(file), {});
		assert(!Encoded.empty());
		auto* result = ReadPlayerInfo(nullptr);
		assert(result);
		auto* hero = reinterpret_cast<LastPlayerInfo*>(result);
		assert(hero->version == 30 && memchr(hero->CharName, 0, sizeof(hero->CharName)));
		cout << "PASS supplied save: " << hero->CharName << ", version " << hero->version << ", " << sizeof(*hero) << " bytes\n";
		FreeMemZero(result);
		assert(Allocations == 0);
	}
	cout << "All save compatibility and slot protection checks passed.\n";
}
