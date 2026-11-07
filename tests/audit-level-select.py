"""Exercise actual level-start/save-routing methods against isolated dependencies.

Uses the real RegEmu persistence across processes. This verifies state ownership,
not rendering or iPad touch behavior. No production save or registry is accessed.
"""
from pathlib import Path
import subprocess
import tempfile

source = Path(__file__).resolve().parents[1]


def method(path, signature):
    text = (source / path).read_text()
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


methods = '\n'.join(method('src/LawnApp.cpp', sig) for sig in [
    'int LawnApp::GetAdventureStars(', 'bool LawnApp::HasAdventureDefenseBadge(', 'void LawnApp::RecordAdventureStars()',
    'int LawnApp::GetPendingAdventureStars(', 'void LawnApp::ClearPendingAdventureStars(',
    'void LawnApp::StartAdventureLevel(', 'void LawnApp::PreNewGame(',
    'void LawnApp::KillBoard()',
])
methods += '\n' + method('src/Lawn/Board.cpp', 'void Board::TryToSaveGame()')
methods += '\n' + method('src/Lawn/System/PlayerInfo.cpp', 'void PlayerInfo::DeleteUserFiles()')

fixture = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <format>
#include <map>
#include <string>
#include <vector>
#include "ConstEnums.h"
#include "Lawn/AdventureProgress.h"
#include "SexyAppFramework/misc/RegEmu.h"
using namespace std;
struct LawnApp;
struct PlayerInfo {int mId=7,mLevel=22,mFinishedAdventure=0;void DeleteUserFiles();};
struct Music {void GameMusicPause(bool){}};
struct WidgetManager {void RemoveWidget(void*){}};
struct Board {
 LawnApp* mApp=nullptr;
 int mLevel=1,mTriggeredLawnMowers=0,mBoardFadeOutCounter=-1;
 bool mIsAdventureReplay=false,mAdventurePostClearRun=false,mLevelAwardSpawned=false,saveNeeded=true;
 bool NeedSaveGame(){return saveNeeded;} void TryToSaveGame();
 void CompleteEndLevelSequenceForSaving(){} void SurvivalSaveScore(){} void DisposeBoard(){}
 bool LoadGame(const string&);
};
map<string,pair<int,bool>> saves;
vector<string> erased,written;
string GetSavedGameName(GameMode mode,int id){return format("main{}_{}",id,int(mode));}
string GetLegacySavedGameName(GameMode mode,int id){return format("legacy{}_{}",id,int(mode));}
string GetAdventureReplaySaveName(int id,int level){return format("replay{}_{}",id,level);}
string GetAppDataPath(const string& s){return s;}
constexpr int CURSOR_POINTER=0;
void MkDir(const string&){} void SafeDeleteWidget(Board* b){delete b;}
bool LawnSaveGame(Board* b,const string& s){written.push_back(s);saves[s]={b->mLevel,b->mIsAdventureReplay};return true;}
struct LawnApp {
 PlayerInfo* mPlayerInfo=nullptr;Board* mBoard=nullptr;int mAdventureReplayLevel=0;
 GameMode mGameMode=GAMEMODE_ADVENTURE;BoardResult mBoardResult=BOARDRESULT_NONE;
 bool mFirstTimeGameSelector=true,continued=false;int newGames=0,selectorKills=0;
 Music music;Music* mMusic=&music;WidgetManager manager;WidgetManager* mWidgetManager=&manager;
 bool IsAdventureMode(){return mGameMode==GAMEMODE_ADVENTURE;}
 bool RegistryReadInteger(const string& key,int* n){uint32_t type=0,len=4;return regemu::RegistryRead("test",key,&type,reinterpret_cast<uint8_t*>(n),&len)&&type==regemu::REGEMU_DWORD;}
 bool RegistryWriteInteger(const string& key,int n){return regemu::RegistryWrite("test",key,regemu::REGEMU_DWORD,reinterpret_cast<const uint8_t*>(&n),4);}
 bool RegistryReadBoolean(const string& key,bool* value){int n=0;if(!RegistryReadInteger(key,&n))return false;*value=n!=0;return true;}
 bool RegistryWriteBoolean(const string& key,bool value){return RegistryWriteInteger(key,value);}
 void RegistryEraseValue(const string& key){regemu::RegistryEraseValue("test",key);}
 int GetAdventureStars(int);bool HasAdventureDefenseBadge(int);void RecordAdventureStars();void StartAdventureLevel(int);
 int GetPendingAdventureStars(int);void ClearPendingAdventureStars(int,int);
 void PreNewGame(GameMode,bool);void KillBoard();
 void KillLevelSelector(){++selectorKills;} void DoContinueDialog(){continued=true;}
 bool FileExists(const string& s){return saves.contains(s);}
 void EraseFile(const string& s){erased.push_back(s);saves.erase(s);}
 void FinishModelessDialogs(){}void KillSeedChooserScreen(){}void SetCursor(int){}void ClearUpdateBacklog(){}
 void MakeNewBoard(){KillBoard();mBoard=new Board;mBoard->mApp=this;}
 void NewGame(){MakeNewBoard();++newGames;mBoard->mLevel=mAdventureReplayLevel?mAdventureReplayLevel:mPlayerInfo->mLevel;mBoard->mIsAdventureReplay=mAdventureReplayLevel>0;mBoardResult=BOARDRESULT_NONE;}
 bool TryLoadGame(){const auto s=GetSavedGameName(mGameMode,mPlayerInfo->mId);if(!FileExists(s))return false;MakeNewBoard();return mBoard->LoadGame(s);}
};
LawnApp* gSexyAppBase=nullptr;
bool Board::LoadGame(const string& s){auto it=saves.find(s);if(it==saves.end())return false;mLevel=it->second.first;mIsAdventureReplay=it->second.second;return true;}
__METHODS__
int main(int argc,char** argv){
 assert(argc==3);regemu::SetRegFile(argv[1]);PlayerInfo p;LawnApp a;a.mPlayerInfo=&p;gSexyAppBase=&a;
 if(string(argv[2])=="write"){
  a.NewGame();a.mBoard->mTriggeredLawnMowers=2;a.RecordAdventureStars();assert(a.GetAdventureStars(22)==1);
  assert(a.GetPendingAdventureStars(22)==0);
  a.mBoard->mTriggeredLawnMowers=1;a.RecordAdventureStars();assert(a.GetAdventureStars(22)==2);
  a.mBoard->mTriggeredLawnMowers=0;a.RecordAdventureStars();assert(a.GetAdventureStars(22)==3);
  assert(!a.HasAdventureDefenseBadge(22));
  a.mBoard->mTriggeredLawnMowers=5;a.RecordAdventureStars();assert(a.GetAdventureStars(22)==3);
  assert(a.GetPendingAdventureStars(22)==0);
  a.KillBoard();return 0;
 }
 if(string(argv[2])=="earn"){
  assert(a.GetAdventureStars(22)==3&&!a.HasAdventureDefenseBadge(22));
  assert(a.GetPendingAdventureStars(22)==0); // Arrival survives a process restart.
  a.ClearPendingAdventureStars(22,2);assert(a.GetPendingAdventureStars(22)==0);
  a.ClearPendingAdventureStars(22,3);
  a.NewGame();a.mBoard->mAdventurePostClearRun=true;
  a.mBoard->mTriggeredLawnMowers=1;a.RecordAdventureStars();assert(!a.HasAdventureDefenseBadge(22));
  a.mBoard->mTriggeredLawnMowers=0;a.RecordAdventureStars();assert(a.HasAdventureDefenseBadge(22));
  assert(a.GetAdventureStars(22)==3&&a.GetPendingAdventureStars(22)==-1);
  a.KillBoard();return 0;
 }
 assert(a.GetAdventureStars(22)==3);assert(a.GetAdventureStars(1)==0);
 assert(a.HasAdventureDefenseBadge(22)); // Badge survives a process restart.
 a.NewGame();a.mBoard->mTriggeredLawnMowers=2;a.RecordAdventureStars();
 assert(a.GetPendingAdventureStars(22)==-1);a.KillBoard(); // A lower run earns no new star.
 p.mId=8;assert(a.GetAdventureStars(22)==0&&a.GetPendingAdventureStars(22)==-1&&!a.HasAdventureDefenseBadge(22));p.mId=7;
 assert(a.GetAdventureStars(0)==0&&a.GetAdventureStars(51)==0);
 for(int level=1;level<=50;++level){assert(AdventureProgress::CanSelect(level,22,0)==(level<=22));assert(AdventureProgress::CanSelect(level,1,1));}
 assert(!AdventureProgress::CanSelect(0,50,1)&&!AdventureProgress::CanSelect(51,50,1));
 assert(AdventureProgress::UnlockedLevel(0,0)==1&&AdventureProgress::UnlockedLevel(999,0)==50);
 // Locked levels cannot kill the selector, change progress or start a board.
 a.StartAdventureLevel(23);a.StartAdventureLevel(0);assert(!a.mBoard&&a.selectorKills==0&&p.mLevel==22);
 saves[GetSavedGameName(GAMEMODE_ADVENTURE,p.mId)]={22,false};
 a.StartAdventureLevel(8);assert(a.mBoard&&a.mBoard->mLevel==8&&a.mBoard->mIsAdventureReplay&&p.mLevel==22);
 a.mBoard->TryToSaveGame();assert(written.back()==GetAdventureReplaySaveName(p.mId,8));
 assert(saves[GetSavedGameName(GAMEMODE_ADVENTURE,p.mId)].first==22);
 // Restart leaves the main and legacy saves alone, stays on the selected level.
 a.mBoardResult=BOARDRESULT_RESTART;a.KillBoard();a.PreNewGame(GAMEMODE_ADVENTURE,false);
 assert(a.mBoard->mLevel==8&&a.mBoard->mIsAdventureReplay&&p.mLevel==22);
 for(const auto& s:erased)assert(!s.starts_with("main")&&!s.starts_with("legacy"));
 a.mBoard->TryToSaveGame();a.mBoardResult=BOARDRESULT_QUIT;a.KillBoard();
 a.mAdventureReplayLevel=0;a.StartAdventureLevel(8);assert(a.continued&&a.mBoard->mIsAdventureReplay&&a.mBoard->mLevel==8);
 a.mBoardResult=BOARDRESULT_WON;a.KillBoard();assert(!saves.contains(GetAdventureReplaySaveName(p.mId,8)));
 assert(saves.contains(GetSavedGameName(GAMEMODE_ADVENTURE,p.mId)));
 // The current main level resumes its own save and resets the replay override.
 a.StartAdventureLevel(22);assert(a.mAdventureReplayLevel==0&&a.mBoard->mLevel==22&&!a.mBoard->mIsAdventureReplay);
 a.mBoardResult=BOARDRESULT_NONE;a.mGameMode=GAMEMODE_CHALLENGE_WALLNUT_BOWLING;
 a.mBoard->mLevel=1;a.mBoard->mAdventurePostClearRun=true;
 a.RecordAdventureStars();assert(a.GetAdventureStars(1)==0&&!a.HasAdventureDefenseBadge(1));
 a.mGameMode=GAMEMODE_ADVENTURE;a.mBoardResult=BOARDRESULT_CHEAT;
 a.RecordAdventureStars();assert(a.GetAdventureStars(1)==0&&!a.HasAdventureDefenseBadge(1));
 a.mBoardResult=BOARDRESULT_NONE;a.KillBoard();
 p.DeleteUserFiles();assert(a.GetAdventureStars(22)==0);
 assert(a.GetPendingAdventureStars(22)==-1&&!a.HasAdventureDefenseBadge(22));
 puts("PASS actual methods: profile-scoped best stars and defense badge, process persistence, unlock bounds, replay save routing and cleanup, profile deletion; ASan/UBSan");
}
'''.replace('__METHODS__', methods)

with tempfile.TemporaryDirectory(prefix='pvz-level-select-') as directory:
    directory = Path(directory)
    (directory / 'Common.h').write_text('''#pragma once
#include <filesystem>
#include <string_view>
namespace Sexy {
inline std::filesystem::path PathFromU8(const std::string& p){return std::filesystem::path(p);}
template<class... T> void LogInfoLn(std::string_view,T&&...){}
}
''')
    cpp = directory / 'level-select.cpp'
    cpp.write_text(fixture)
    binary = directory / 'level-select'
    subprocess.run(['xcrun', 'clang++', '-std=c++20', '-fsanitize=address,undefined',
                    '-I' + str(directory), '-I' + str(source / 'src'), str(cpp),
                    str(source / 'src/SexyAppFramework/misc/RegEmu.cpp'), '-o', str(binary)], check=True)
    for phase in ['write', 'earn', 'read-and-test']:
        subprocess.run([str(binary), str(directory / 'settings.regemu'), phase], check=True, timeout=15)
