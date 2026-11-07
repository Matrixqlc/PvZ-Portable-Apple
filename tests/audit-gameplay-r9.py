"""Run real gameplay methods with controlled dependencies and the real RegEmu file store.

This is a host-side behavior check, not an iPad rendering or device test.
"""
from pathlib import Path
import subprocess
import tempfile

source = Path(__file__).resolve().parents[1]


def method(path, signature):
    text = (source / path).read_text()
    start = text.index(signature)
    cursor = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[cursor] == '{') - (text[cursor] == '}')
        cursor += 1
    return text[start:cursor]


methods = '\n'.join(method('src/LawnApp.cpp', signature) for signature in [
    'void LawnApp::ReadFromRegistry()', 'void LawnApp::WriteToRegistry()',
    'void LawnApp::SetAutoCollectSun(', 'void LawnApp::SetAutoCollectMoney(',
    'void LawnApp::SetShowHealth(', 'void LawnApp::SetGameSpeedHalfUnits(',
    'void LawnApp::UpdateFrames()',
])
methods += '\n' + '\n'.join(method('src/Lawn/Coin.cpp', signature) for signature in [
    'bool Coin::IsSun()', 'bool Coin::IsMoney(CoinType', 'bool Coin::IsMoney()',
    'void Coin::Update()', 'void Coin::MouseDown(',
])
methods += '\n' + method('src/Lawn/Widget/NewOptionsDialog.cpp', 'void NewOptionsDialog::Resize(')
methods += '\n' + method('src/Lawn/Widget/NewOptionsDialog.cpp', 'std::string_view GameSpeedLabel(')
header = (source / 'src/LawnApp.h').read_text().splitlines()
defaults = '\n'.join(line for line in header if any(name in line for name in [
    'mAutoCollectSun =', 'mAutoCollectMoney =', 'mShowHealth =',
    'mGameSpeedHalfUnits =', 'mGameSpeedRemainder =']))

fixture = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <string_view>
#include "ConstEnums.h"
#include "SexyAppFramework/misc/RegEmu.h"
#ifdef PVZ_TEST_IOS
#define __IPHONEOS__ 1
#endif
std::string registryPath;
std::function<void()> stepHook;
int baseSteps=0;
struct SexyApp {
    void ReadFromRegistry(){regemu::SetRegFile(registryPath);}
    void WriteToRegistry(){RegistryWriteInteger("MusicVolume",85);}
    void UpdateFrames(){++baseSteps;if(stepHook)stepHook();}
    bool RegistryReadInteger(const std::string& k,int* value){
        uint32_t type=0,length=sizeof(int);int n=0;
        if(!regemu::RegistryRead("game",k,&type,reinterpret_cast<uint8_t*>(&n),&length)||type!=regemu::REGEMU_DWORD)return false;
        *value=n;return true;
    }
    bool RegistryReadBoolean(const std::string& k,bool* v){int n=0;if(!RegistryReadInteger(k,&n))return false;*v=n!=0;return true;}
    bool RegistryWriteInteger(const std::string& k,int v){return regemu::RegistryWrite("game",k,regemu::REGEMU_DWORD,reinterpret_cast<const uint8_t*>(&v),sizeof(v));}
    bool RegistryWriteBoolean(const std::string& k,bool v){return RegistryWriteInteger(k,v?1:0);}
    bool RegistryWriteString(const std::string& k,const std::string& v){return regemu::RegistryWrite("game",k,regemu::REGEMU_SZ,reinterpret_cast<const uint8_t*>(v.data()),static_cast<uint32_t>(v.size()));}
};
using SexyAppBase=SexyApp;
struct CutScene{bool ShouldRunUpsellBoard(){return false;}};
struct Board {
    bool mPaused=false;int mLevel=1,deleted=0,advice=0;CutScene scene;CutScene* mCutScene=&scene;
    void ResetFPSStats(){} void ProcessDeleteQueue(){++deleted;}
    void DisplayAdvice(const char*,MessageStyle,AdviceType){++advice;}
};
struct Effect{void ProcessDeleteQueue(){}};
struct Music{int updates=0;void MusicUpdate(){++updates;}};
struct Player {std::string mName="test";void SaveDetails(){}};
bool gSlowMo=false,gFastMo=false;int gSlowMoCounter=0;
struct LawnApp:SexyApp {
    __DEFAULTS__
    bool mActive=true,mMinimized=false,mLoadingThreadCompleted=false,modal=false;
    Board* mBoard=nullptr;Effect* mEffectSystem=nullptr;Music music;Music* mMusic=&music;
    Player* mPlayerInfo=nullptr;int mAppCounter=0;bool firstAdventure=true;
    GameScenes mGameScene=GameScenes::SCENE_PLAYING;GameMode mGameMode=GameMode::GAMEMODE_ADVENTURE;
    void ReadFromRegistry();void WriteToRegistry();void SetAutoCollectSun(bool);void SetAutoCollectMoney(bool);void SetShowHealth(bool);void SetGameSpeedHalfUnits(int);void UpdateFrames();
    bool NeedPauseGame(){return modal;}void CheckForGameEnd(){}
    bool IsFirstTimeAdventureMode(){return firstAdventure;}
};
struct Color{Color(int,int,int,int){}};
struct Coin {
    LawnApp* mApp;Board* mBoard;CoinType mType=CoinType::COIN_SUN;
    bool mDead=false,mIsBeingCollected=false,mHitGround=false;
    int mCoinAge=0,mFadeCount=0,collects=0,sounds=0,fall=0,collectedUpdates=0;
    float mPosX=0,mPosY=0,mScale=1;
    AttachmentID mAttachmentID=AttachmentID::ATTACHMENTID_NULL;
    bool IsSun();static bool IsMoney(CoinType);bool IsMoney();void Update();void MouseDown(int,int,int);
    void Collect(){++collects;mIsBeingCollected=true;mFadeCount=0;}
    void PlayCollectSound(){++sounds;}void UpdateFade(){--mFadeCount;}
    void UpdateFall(){++fall;}void UpdateCollected(){++collectedUpdates;}
    Color GetColor(){return Color(0,0,0,0);}
};
void AttachmentUpdateAndMove(AttachmentID,float,float){}
void AttachmentOverrideColor(AttachmentID,Color){}
void AttachmentOverrideScale(AttachmentID,float){}
struct W {int mX=0,mY=0,mWidth=100,mHeight=33;
    void Resize(int x,int y,int w,int h){mX=x;mY=y;mWidth=w;mHeight=h;}};
struct Dialog {void Resize(int,int,int,int){}};
struct NewOptionsDialog:Dialog {
    LawnApp* mApp;bool mFromGameSelector=false;
    W a,b,c,d,e,f,g,h,autoSun,speed,money,health;
    W *mMusicVolumeSlider=&a,*mSfxVolumeSlider=&b,*mHardwareAccelerationCheckbox=&c,*mFullscreenCheckbox=&d,
      *mAlmanacButton=&e,*mRestartButton=&f,*mBackToMainButton=&g,*mBackToGameButton=&h,
      *mAutoCollectSunCheckbox=&autoSun,*mGameSpeedButton=&speed,
      *mAutoCollectMoneyCheckbox=&money,*mShowHealthCheckbox=&health;
    void Resize(int,int,int,int);
};
__METHODS__
void tick(LawnApp& app,int n){for(int i=0;i<n;++i)app.UpdateFrames();}
int main(int argc,char** argv){
    assert(argc==3);registryPath=argv[1];const std::string phase=argv[2];
    if(phase=="write"){
        LawnApp app;app.ReadFromRegistry();assert(!app.mAutoCollectSun&&!app.mAutoCollectMoney&&!app.mShowHealth&&app.mGameSpeedHalfUnits==2);
        app.SetAutoCollectSun(true);app.SetAutoCollectMoney(true);app.SetShowHealth(true);app.SetGameSpeedHalfUnits(3);
        // No shutdown save: setters must already have written the file.
        return 0;
    }
    LawnApp restored;restored.ReadFromRegistry();assert(restored.mAutoCollectSun&&restored.mAutoCollectMoney&&restored.mShowHealth&&restored.mGameSpeedHalfUnits==3);
    restored.WriteToRegistry();int volume=0;assert(restored.RegistryReadInteger("MusicVolume",&volume)&&volume==85);
    restored.SetGameSpeedHalfUnits(0);assert(restored.mGameSpeedHalfUnits==1);
    restored.SetGameSpeedHalfUnits(99);assert(restored.mGameSpeedHalfUnits==4);
    restored.SetAutoCollectSun(false);restored.SetAutoCollectMoney(false);restored.SetShowHealth(false);LawnApp off;off.ReadFromRegistry();assert(!off.mAutoCollectSun&&!off.mAutoCollectMoney&&!off.mShowHealth);
    Board board,another;LawnApp app;app.mBoard=&board;
    for(int units:{1,2,3,4}){
        app.SetGameSpeedHalfUnits(units);int before=baseSteps;tick(app,200);
        assert(baseSteps-before==100*units);
        assert(app.mGameSpeedHalfUnits==units);
    }
    // Menus, pauses, background and modal dialogs retain normal UI cadence.
    app.SetGameSpeedHalfUnits(4);
    for(int scenario=0;scenario<6;++scenario){
        app.mBoard=&board;app.mGameScene=GameScenes::SCENE_PLAYING;board.mPaused=false;
        app.modal=false;app.mActive=true;app.mMinimized=false;
        if(scenario==0)app.mGameScene=GameScenes::SCENE_MENU;
        if(scenario==1)board.mPaused=true;
        if(scenario==2)app.modal=true;
        if(scenario==3)app.mActive=false;
        if(scenario==4)app.mMinimized=true;
        if(scenario==5)app.mBoard=nullptr;
        int before=baseSteps;tick(app,20);assert(baseSteps-before==20);
    }
    app.mBoard=&board;app.mGameScene=GameScenes::SCENE_PLAYING;app.mMinimized=false;
    for(int transition=0;transition<4;++transition){
        board.mPaused=false;app.modal=false;app.mBoard=&board;app.mGameScene=GameScenes::SCENE_PLAYING;
        stepHook=[&]{if(transition==0)app.mGameScene=GameScenes::SCENE_AWARD;
            if(transition==1)app.mBoard=&another;if(transition==2)board.mPaused=true;if(transition==3)app.modal=true;};
        int before=baseSteps;app.UpdateFrames();assert(baseSteps-before==1);stepHook={};
    }
    // Replacing a board keeps the selected setting; half steps do not drift.
    app.modal=false;board.mPaused=false;app.mBoard=&another;app.mGameScene=GameScenes::SCENE_PLAYING;
    app.SetGameSpeedHalfUnits(3);int before=baseSteps;tick(app,200);assert(baseSteps-before==300);
    gSlowMo=true;gSlowMoCounter=0;before=baseSteps;tick(app,20);assert(baseSteps-before==5);gSlowMo=false;
    gFastMo=true;before=baseSteps;tick(app,2);assert(baseSteps-before==40);gFastMo=false;
    app.mBoard=&board;app.SetAutoCollectSun(true);
    for(auto type:{CoinType::COIN_SUN,CoinType::COIN_SMALLSUN,CoinType::COIN_LARGESUN}){
        Coin coin{&app,&board};coin.mType=type;coin.Update();coin.Update();
        assert(coin.collects==1&&coin.sounds==1&&coin.collectedUpdates==2);
    }
    Coin money{&app,&board};money.mType=CoinType::COIN_GOLD;money.Update();assert(money.collects==0);
    Coin paused{&app,&board};board.mPaused=true;paused.Update();assert(paused.collects==0);board.mPaused=false;
    Coin dead{&app,&board};dead.mDead=true;dead.Update();assert(dead.collects==0);
    Coin awarded{&app,&board};app.mGameScene=GameScenes::SCENE_AWARD;awarded.Update();assert(awarded.collects==0);
    app.mGameScene=GameScenes::SCENE_PLAYING;Coin fading{&app,&board};fading.mFadeCount=1;fading.Update();assert(fading.collects==1&&fading.mFadeCount==0);
    app.SetAutoCollectSun(false);Coin manual{&app,&board};manual.Update();assert(manual.collects==0);manual.MouseDown(0,0,1);assert(manual.collects==1);
    assert(board.advice==5); // three auto suns, fading sun, and manual sun.
    app.firstAdventure=false;app.SetAutoCollectMoney(true);
    for(auto type:{CoinType::COIN_SILVER,CoinType::COIN_GOLD,CoinType::COIN_DIAMOND}){
        Coin coin{&app,&board};coin.mType=type;coin.Update();coin.Update();
        assert(coin.collects==1&&coin.sounds==1&&coin.collectedUpdates==2);
    }
    for(auto type:{CoinType::COIN_SUN,CoinType::COIN_TROPHY,CoinType::COIN_AWARD_MONEY_BAG,
                   CoinType::COIN_PRESENT_PLANT,CoinType::COIN_USABLE_SEED_PACKET}){
        Coin coin{&app,&board};coin.mType=type;coin.Update();assert(coin.collects==0);
    }
    for(int scenario=0;scenario<5;++scenario){
        Coin coin{&app,&board};coin.mType=CoinType::COIN_GOLD;
        if(scenario==0)board.mPaused=true;
        if(scenario==1)app.mGameScene=GameScenes::SCENE_AWARD;
        if(scenario==2)coin.mDead=true;
        if(scenario==3)coin.mIsBeingCollected=true;
        if(scenario==4)coin.mBoard=nullptr;
        coin.Update();assert(coin.collects==0);
        board.mPaused=false;app.mGameScene=GameScenes::SCENE_PLAYING;
    }
    app.SetAutoCollectMoney(false);money.Update();assert(money.collects==0);
    money.MouseDown(0,0,1);assert(money.collects==1);
    NewOptionsDialog dialog;dialog.mApp=&app;
    for(bool selector:{false,true}){
        dialog.mFromGameSelector=selector;dialog.Resize(0,0,423,498);
        assert(dialog.autoSun.mY+dialog.autoSun.mHeight<=dialog.speed.mY);
        if(!selector)assert(dialog.speed.mY+dialog.speed.mHeight<dialog.e.mY);
        assert(dialog.speed.mY+dialog.speed.mHeight<dialog.h.mY);
        assert(dialog.speed.mY+dialog.speed.mHeight<=dialog.health.mY);
        assert(dialog.health.mX==dialog.autoSun.mX&&dialog.money.mX==dialog.autoSun.mX);
        assert(dialog.health.mY+dialog.health.mHeight<=dialog.money.mY);
#if defined(__IPHONEOS__)
        assert(dialog.h.mY+dialog.h.mHeight<=580);
#else
        assert(dialog.speed.mX==dialog.autoSun.mX);
        assert(dialog.a.mY+dialog.a.mHeight<=dialog.b.mY);
        assert(dialog.b.mY+dialog.b.mHeight<=dialog.autoSun.mY);
#endif
        if(!selector)assert(dialog.money.mY+dialog.money.mHeight<dialog.e.mY);
        assert(dialog.g.mY+dialog.g.mHeight<=dialog.h.mY);
#if defined(__APPLE__) && !defined(__IPHONEOS__)
        assert(dialog.money.mY+dialog.money.mHeight<=dialog.c.mY);
        assert(dialog.c.mX+dialog.c.mWidth<=dialog.d.mX);
        if(!selector)assert(dialog.d.mY+dialog.d.mHeight<dialog.e.mY);
        assert(dialog.h.mY+dialog.h.mHeight<=600);
#endif
    }
    assert(GameSpeedLabel(1)=="0.5x"&&GameSpeedLabel(2)=="1x"&&GameSpeedLabel(3)=="1.5x"&&GameSpeedLabel(4)=="2x");
    puts("PASS real gameplay methods: four speed ratios, pause/background/menu, level transitions, cross-board settings, sun/money types, independent toggles, manual/tutorial/pause/reward guards, disk persistence across processes, options geometry; ASan/UBSan");
}
'''.replace('__METHODS__', methods).replace('__DEFAULTS__', defaults)

with tempfile.TemporaryDirectory(prefix='pvz-gameplay-r9-') as directory:
    directory = Path(directory)
    (directory / 'Common.h').write_text('''#pragma once
#include <filesystem>
#include <string_view>
namespace Sexy {
inline std::filesystem::path PathFromU8(const std::string& p){return std::filesystem::path(p);}
template<class... T> void LogInfoLn(std::string_view,T&&...){}
}
''')
    fixture_path = directory / 'gameplay.cpp'
    fixture_path.write_text(fixture)
    binary = directory / 'gameplay'
    for platform in ['iOS', 'Mac']:
        flags = ['-DPVZ_TEST_IOS=1'] if platform == 'iOS' else []
        subprocess.run(['xcrun', 'clang++', '-std=c++20', '-fsanitize=address,undefined', *flags,
                        '-I' + str(directory), '-I' + str(source / 'src'),
                        str(fixture_path), str(source / 'src/SexyAppFramework/misc/RegEmu.cpp'),
                        '-o', str(binary)], check=True)
        registry = directory / f'settings-{platform}.regemu'
        for phase in ['write', 'read-and-test']:
            subprocess.run([str(binary), str(registry), phase], check=True, timeout=15)
        print(f'PASS {platform} gameplay controls and options layout', flush=True)
