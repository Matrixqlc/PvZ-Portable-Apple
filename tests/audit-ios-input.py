import sys
from pathlib import Path
import subprocess
r=(Path(sys.argv[1]).resolve() if len(sys.argv)>1 else Path(__file__).resolve().parents[1])/"src"
def extract(path,name):
 s=path.read_text();a=s.index(name);b=s.index('{',a);i=b+1;d=1
 while d:d+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[a:i]
method=extract(r/'SexyAppFramework/platform/default/Input.cpp','bool SexyAppBase::ProcessDeferredMessages(')
focus=extract(r/'SexyAppFramework/SexyAppBase.cpp','void SexyAppBase::RehupFocus()')
pre=r'''#define SDL_MAIN_HANDLED
#define __IPHONEOS__ 1
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <chrono>
#include "SexyAppFramework/misc/Rect.h"
#include <limits>
#include <deque>
#include <cstring>
#include <cassert>
#include <iostream>
#include "SexyAppFramework/misc/KeyCodes.h"
#include "SexyAppFramework/misc/SingleTouchInput.h"
using namespace Sexy;
static unsigned int gIOSInputLogBudget=128,gIOSInputWindowBudget=32;static bool gIOSInputWindowStarted;static int diagnosticCount;
namespace Sexy {template<class...T> void LogInfoLn(const char*,T&&...){++diagnosticCount;}}
static int logicalW=800,logicalH=600,drawableW=800,drawableH=600;
static bool nativeActive=true;static Uint64 nativeEpoch=1;static Uint32 now=100,reset=100;
extern "C" int PVZIsSceneActive(){return nativeActive;}
extern "C" Uint64 PVZSceneInputEpoch(){return nativeEpoch;}
extern "C" Uint32 PVZSceneInputResetTicks(){return reset;}
static SingleTouchInput gIOSTouchInput;
static Uint64 gIOSObservedInputEpoch=std::numeric_limits<Uint64>::max();
static std::deque<SDL_Event> events;
extern "C" int SDL_PollEvent(SDL_Event*e){if(events.empty())return 0;*e=events.front();events.pop_front();return 1;}
extern "C" SDL_bool SDL_HasEvents(Uint32,Uint32){return events.empty()?SDL_FALSE:SDL_TRUE;}
extern "C" Uint32 SDL_GetTicks(){return now;}
extern "C" Uint32 SDL_GetMouseState(int*x,int*y){if(x)*x=0;if(y)*y=0;return 0;}
extern "C" Uint32 SDL_GetWindowID(SDL_Window*){return 7;}
extern "C" void SDL_GetWindowSize(SDL_Window*,int*w,int*h){*w=logicalW;*h=logicalH;}
extern "C" void SDL_GL_GetDrawableSize(SDL_Window*,int*w,int*h){*w=drawableW;*h=drawableH;}
extern "C" void* SDL_memset(void*p,int c,size_t n){return std::memset(p,c,n);}
struct FakeWidgets {
 Rect mMouseDestRect{0,0,800,600},mMouseSourceRect{0,0,800,600};int mDownButtons=0;
 bool mHasFocus=true,down=false;int mLastMouseX=0,mLastMouseY=0,clicks=0,downs=0,cancels=0;
 void GotFocus(){mHasFocus=true;} void LostFocus(){mHasFocus=false;}
 void DoMouseUps(){if(down){if(mHasFocus)++clicks;else ++cancels;}down=false;mDownButtons=0;}
 void Resize(const Rect&d,const Rect&s){mMouseDestRect=d;mMouseSourceRect=s;}void MarkAllDirty(){}void RemapMouse(int&,int&);
 void MouseMove(int x,int y){mLastMouseX=x;mLastMouseY=y;}
 void MouseDown(int,int,int){down=true;mDownButtons=1;++downs;}
 void MouseUp(int,int,int){if(down&&mHasFocus)++clicks;down=false;mDownButtons=0;}
 void MouseExit(int,int){}void MouseWheel(int){}void KeyDown(KeyCode){}void KeyUp(KeyCode){}void KeyChar(char){}void KeyText(std::string_view){}
};
struct SexyAppBase;
struct FakeGL {SexyAppBase*mApp=nullptr;Rect mPresentationRect;void UpdateViewport();};
static void GfxFlush(){}static void glViewport(int,int,int,int){}
struct Demo {void WriteNumBits(int,int){}void WriteString(const std::string&) {}};
struct SexyAppBase {
 bool mActive=true,mMinimized=false,mHasFocus=true,mMuteOnLostFocus=false;
 bool mPlayingDemoBuffer=false,mRecordingDemoBuffer=false,mShutdown=false,mMouseIn=true,mAllowAltEnter=false,mIsWindowed=false;
 Uint32 mLastTimeCheck=0,mLastUserInputTick=0,mLastTimerTime=0;double mUpdateFTimeAcc=0;Rect mScreenBounds{0,0,800,600};
 void*mWindow=reinterpret_cast<void*>(1);FakeWidgets*mWidgetManager;FakeGL*mGLInterface;Demo mDemoBuffer;
 void Mute(bool){}void Unmute(bool){}void GotFocus(){}void LostFocus(){}void EnforceCursor(){}void SwitchScreenMode(bool){}void CloseRequestAsync(){}void WriteDemoTimingBlock(){}
 void RehupFocus();bool ProcessDeferredMessages(bool);
};
static void RecordDemoEvent(SexyAppBase*,const SDL_Event&){}
static KeyCode SDLKeyToKeyCode(SDL_Keycode){return KEYCODE_UNKNOWN;}
static bool SDLSynthesizeAsciiCharFromKeyDown(const SDL_KeyboardEvent&,char&){return false;}
enum{DEMO_KEY_DOWN,DEMO_KEY_TEXT};
'''
post=r'''
static SDL_Event touch(Uint32 type,Sint64 id,Uint32 timestamp){SDL_Event e{};e.type=type;e.tfinger.type=type;e.tfinger.timestamp=timestamp;e.tfinger.touchId=1;e.tfinger.fingerId=id;e.tfinger.windowID=7;e.tfinger.x=.5f;e.tfinger.y=.5f;return e;}
static void send(SexyAppBase&a,SDL_Event e){events.push_back(e);a.ProcessDeferredMessages(true);}
static void transition(SexyAppBase&a,bool active){nativeActive=active;++nativeEpoch;now+=10;reset=now;a.ProcessDeferredMessages(true);}
int main(){FakeWidgets w;FakeGL gl;SexyAppBase a;a.mWidgetManager=&w;a.mGLInterface=&gl;gl.mApp=&a;
 a.ProcessDeferredMessages(true);send(a,touch(SDL_FINGERDOWN,1,now+1));send(a,touch(SDL_FINGERUP,1,now+2));assert(w.clicks==1);
 send(a,touch(SDL_FINGERDOWN,2,now+3));transition(a,false);assert(!w.down&&!w.mHasFocus&&w.clicks==1&&w.cancels==1);
 send(a,touch(SDL_FINGERUP,2,now+1));assert(w.clicks==1);transition(a,true);
 send(a,touch(SDL_FINGERDOWN,3,reset-1));send(a,touch(SDL_FINGERUP,3,reset-1));assert(w.clicks==1);
 for(int i=0;i<100;++i){transition(a,false);transition(a,true);send(a,touch(SDL_FINGERDOWN,100+i,now+1));send(a,touch(SDL_FINGERUP,100+i,now+2));}
 assert(w.clicks==101);
 // Secondary fingers do not steal capture; the primary release still activates once.
 send(a,touch(SDL_FINGERDOWN,900,now+3));send(a,touch(SDL_FINGERDOWN,901,now+4));send(a,touch(SDL_FINGERUP,901,now+5));send(a,touch(SDL_FINGERUP,900,now+6));assert(w.clicks==102);
 // Even a complete inactive/active cycle inside a UIKit poll is observable by epoch.
 send(a,touch(SDL_FINGERDOWN,910,now+7));nativeEpoch+=2;now+=10;reset=now;nativeActive=true;a.ProcessDeferredMessages(true);assert(!w.down&&w.cancels==2&&w.clicks==102);
 send(a,touch(SDL_FINGERDOWN,911,now+1));send(a,touch(SDL_FINGERUP,911,now+2));assert(w.clicks==103);
 // Invalid points/window, mouse-derived fingers and legacy synthesized mouse events are ignored.
 auto invalid=touch(SDL_FINGERDOWN,920,now+3);invalid.tfinger.x=std::numeric_limits<float>::quiet_NaN();send(a,invalid);
 invalid=touch(SDL_FINGERDOWN,921,now+3);invalid.tfinger.windowID=8;send(a,invalid);
 invalid=touch(SDL_FINGERDOWN,922,now+3);invalid.tfinger.touchId=SDL_MOUSE_TOUCHID;send(a,invalid);
 SDL_Event legacy{};legacy.type=SDL_MOUSEBUTTONDOWN;legacy.button.which=SDL_TOUCH_MOUSEID;send(a,legacy);assert(!w.down);
 // Native inactive state wins over stale focus-restored events.
 transition(a,false);SDL_Event stale{};stale.type=SDL_WINDOWEVENT;stale.window.event=SDL_WINDOWEVENT_FOCUS_GAINED;send(a,stale);assert(!a.mHasFocus);transition(a,true);

 // The old point-based conversion would map the 2x drawable center to ~184,150.
 logicalW=1180;logicalH=820;
 for(auto size : {std::pair{1180,820},std::pair{2360,1640},std::pair{1600,1200}}){
  drawableW=size.first;drawableH=size.second;
  send(a,touch(SDL_FINGERDOWN,1001,now+1));send(a,touch(SDL_FINGERUP,1001,now+2));
  assert(std::abs(w.mLastMouseX-400)<=1&&std::abs(w.mLastMouseY-300)<=1);
 }
 auto clicksBefore=w.clicks;send(a,touch(SDL_FINGERDOWN,1100,now+3));drawableW=drawableH=0;send(a,touch(SDL_FINGERUP,1100,now+4));assert(!w.down&&w.clicks==clicksBefore);
 drawableW=2360;drawableH=1640;send(a,touch(SDL_FINGERDOWN,1101,now+5));send(a,touch(SDL_FINGERUP,1101,now+6));assert(w.clicks==clicksBefore+1);
 assert(diagnosticCount<=256);
 // Sampling stays quiet after a busy initial window, then resumes on activation.
 gIOSInputLogBudget=128;gIOSInputWindowBudget=32;gIOSInputWindowStarted=false;diagnosticCount=0;
 for(int i=0;i<100;++i){send(a,touch(SDL_FINGERDOWN,2000+i,now+1));send(a,touch(SDL_FINGERUP,2000+i,now+2));}
 assert(gIOSInputLogBudget==96&&gIOSInputWindowBudget==0&&diagnosticCount==33);
 gIOSInputWindowStarted=true;transition(a,false);transition(a,true);
 auto n=diagnosticCount;send(a,touch(SDL_FINGERDOWN,3000,now+1));send(a,touch(SDL_FINGERUP,3000,now+2));assert(diagnosticCount==n+2);
 for(int i=0;i<200;++i){transition(a,false);transition(a,true);send(a,touch(SDL_FINGERDOWN,4000+i,now+1));send(a,touch(SDL_FINGERUP,4000+i,now+2));}
 assert(gIOSInputLogBudget==0&&diagnosticCount<=256);
 std::cout<<"PASS: drawable-scale invariant center mapping, zero drawable UP cancellation, bounded initial/resume diagnostic windows, global128 event cap; actual ProcessDeferredMessages/RehupFocus/UpdateViewport/RemapMouse, 100 background cycles, interrupted press cancellation, stale input, multitouch, repeated epochs, invalid coordinates/window and stale focus events\n";
}
'''
viewport=extract(r/'SexyAppFramework/graphics/GLInterface.cpp','void GLInterface::UpdateViewport()').replace('GLInterface::','FakeGL::')
remap=extract(r/'SexyAppFramework/widget/WidgetManager.cpp','void WidgetManager::RemapMouse(').replace('WidgetManager::','FakeWidgets::')
out=Path('/tmp/pvz-ios-input-regression.cpp');out.write_text(pre+viewport+'\n'+remap+'\n'+focus+'\n'+method+'\n'+post)
subprocess.run(['xcrun','clang++','-std=c++20','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wshorten-64-to-32','-Werror','-I','/opt/homebrew/include/SDL2','-I',str(r),'-I',str(r/'SexyAppFramework'),str(out),'-o','/tmp/pvz-ios-input-regression'],check=True)
subprocess.run(['/tmp/pvz-ios-input-regression'],check=True,timeout=15)
