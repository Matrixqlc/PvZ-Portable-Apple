import sys
from pathlib import Path
import subprocess
r=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else Path(__file__).resolve().parents[1];text=(r/'ios/PvZSceneDelegate.m').read_text()
def extract(name,after=0):
 a=text.index(name,after);b=text.index('{',a);i=b+1;depth=1
 while depth:depth+=(text[i]=='{')-(text[i]=='}');i+=1
 return text[a:i]
queue=extract('static dispatch_queue_t PVZAudioQueue(');release=extract('static void PVZReleasePreparedAudio(');take=extract('int PVZTakePreparedAudio(');start=text.index('@implementation PVZSDLAppDelegate');finish=extract('- (void)finishCancelledPreparation:',start);cancel=extract('- (void)cancelPendingStart',start);post=extract('- (void)postFinishLaunch',start)
pre=r'''#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>
#include <stdatomic.h>
#include <assert.h>
static id PVZWindowScene;static BOOL PVZSceneActive,PVZPreparedAudio;
static atomic_int initCalls,openCalls,quitCalls,closeCalls,audioRefs,sdlRefs,mode,gateWaiters;
static atomic_bool gateEnabled;
static int mainCalls;
static dispatch_semaphore_t gate;
#define SDL_INIT_AUDIO 1
#define AUDIO_S16SYS 1
int SDL_InitSubSystem(int flags){(void)flags;assert(!NSThread.isMainThread);atomic_fetch_add(&initCalls,1);if(atomic_load(&gateEnabled)){atomic_fetch_add(&gateWaiters,1);dispatch_semaphore_wait(gate,DISPATCH_TIME_FOREVER);}if(atomic_load(&mode)==1)return -1;atomic_fetch_add(&sdlRefs,1);return 0;}
int Mix_OpenAudio(int f,int format,int channels,int buffer){(void)f;(void)format;(void)channels;(void)buffer;assert(!NSThread.isMainThread);atomic_fetch_add(&openCalls,1);if(atomic_load(&mode)==2)return -1;assert(atomic_load(&audioRefs)==0);atomic_fetch_add(&audioRefs,1);return 0;}
void SDL_QuitSubSystem(int flags){(void)flags;assert(!NSThread.isMainThread);atomic_fetch_add(&quitCalls,1);assert(atomic_fetch_sub(&sdlRefs,1)>0);}
void Mix_CloseAudio(void){assert(!NSThread.isMainThread);atomic_fetch_add(&closeCalls,1);assert(atomic_fetch_sub(&audioRefs,1)>0);}
const char*SDL_GetError(void){return "injected preparation failure";}
'''
classes=r'''
@interface FakeSDLDelegate:NSObject
- (void)postFinishLaunch;
@end
@implementation FakeSDLDelegate
- (void)postFinishLaunch {assert(NSThread.isMainThread);++mainCalls;}
@end
@interface PVZSDLAppDelegate:FakeSDLDelegate
@property(nonatomic,assign)BOOL pvzStarted,pvzAudioReady,pvzRunning;
@property(nonatomic,assign)NSUInteger pvzAudioGeneration;
- (void)cancelPendingStart;
- (void)finishCancelledPreparation:(BOOL)ready;
@end
@implementation PVZSDLAppDelegate
'''
tests=r'''
@end
static void until(BOOL(^condition)(void)){NSDate*deadline=[NSDate dateWithTimeIntervalSinceNow:3];while(!condition()&&deadline.timeIntervalSinceNow>0){[NSRunLoop.currentRunLoop runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:.002]];}assert(condition());}
// Each case owns a different fake delegate. Drain the previous case's run-loop
// continuation while no scene is active, before installing the next fake scene.
static void reset(void){PVZWindowScene=nil;PVZSceneActive=NO;int callsBefore=mainCalls;[NSRunLoop.currentRunLoop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.01]];assert(mainCalls==callsBefore);until(^BOOL{return atomic_load(&audioRefs)==0&&atomic_load(&sdlRefs)==0;});mainCalls=0;atomic_store(&initCalls,0);atomic_store(&openCalls,0);atomic_store(&quitCalls,0);atomic_store(&closeCalls,0);atomic_store(&gateWaiters,0);atomic_store(&mode,0);atomic_store(&gateEnabled,false);PVZPreparedAudio=NO;PVZWindowScene=[NSObject new];PVZSceneActive=YES;}
static void releaseTransferred(void){PVZReleasePreparedAudio(YES,nil);until(^BOOL{return atomic_load(&audioRefs)==0&&atomic_load(&sdlRefs)==0;});}
int main(){@autoreleasepool{gate=dispatch_semaphore_create(0);
 reset();PVZSDLAppDelegate*a=[PVZSDLAppDelegate new];[a postFinishLaunch];[a postFinishLaunch];[a postFinishLaunch];assert(mainCalls==0);until(^BOOL{return a.pvzRunning;});assert(mainCalls==1&&atomic_load(&initCalls)==1&&atomic_load(&openCalls)==1);assert(PVZTakePreparedAudio()==1&&PVZTakePreparedAudio()==0);[a postFinishLaunch];[a cancelPendingStart];assert(a.pvzRunning&&atomic_load(&audioRefs)==1);releaseTransferred();
 reset();atomic_store(&gateEnabled,true);PVZSDLAppDelegate*b=[PVZSDLAppDelegate new];[b postFinishLaunch];until(^BOOL{return atomic_load(&gateWaiters)==1;});PVZSceneActive=NO;atomic_store(&gateEnabled,false);dispatch_semaphore_signal(gate);until(^BOOL{return b.pvzAudioReady;});assert(!b.pvzRunning&&mainCalls==0&&atomic_load(&audioRefs)==1);[b cancelPendingStart];[b cancelPendingStart];until(^BOOL{return !b.pvzStarted&&atomic_load(&audioRefs)==0;});assert(atomic_load(&closeCalls)==1&&!PVZPreparedAudio);
 reset();PVZSDLAppDelegate*c=[PVZSDLAppDelegate new];[c postFinishLaunch];PVZSceneActive=NO;until(^BOOL{return c.pvzAudioReady;});PVZSceneActive=YES;[c postFinishLaunch];assert(c.pvzRunning&&mainCalls==1&&PVZTakePreparedAudio());releaseTransferred();
 reset();atomic_store(&gateEnabled,true);PVZSDLAppDelegate*d=[PVZSDLAppDelegate new];[d postFinishLaunch];until(^BOOL{return atomic_load(&gateWaiters)==1;});[d postFinishLaunch];PVZWindowScene=nil;PVZSceneActive=NO;[d cancelPendingStart];PVZWindowScene=[NSObject new];PVZSceneActive=YES;[d postFinishLaunch];atomic_store(&gateEnabled,false);dispatch_semaphore_signal(gate);until(^BOOL{return d.pvzRunning&&atomic_load(&closeCalls)==1;});assert(mainCalls==1&&atomic_load(&initCalls)==2&&atomic_load(&openCalls)==2&&atomic_load(&audioRefs)==1&&atomic_load(&sdlRefs)==1);assert(PVZTakePreparedAudio());releaseTransferred();
 reset();atomic_store(&gateEnabled,true);PVZSDLAppDelegate*orphan=[PVZSDLAppDelegate new];[orphan postFinishLaunch];until(^BOOL{return atomic_load(&gateWaiters)==1;});PVZWindowScene=nil;PVZSceneActive=NO;[orphan cancelPendingStart];atomic_store(&gateEnabled,false);dispatch_semaphore_signal(gate);until(^BOOL{return !orphan.pvzStarted&&atomic_load(&closeCalls)==1;});assert(mainCalls==0&&!PVZPreparedAudio&&!orphan.pvzAudioReady&&!orphan.pvzRunning&&atomic_load(&audioRefs)==0&&atomic_load(&sdlRefs)==0);
 for(int fail=1;fail<=2;fail++){reset();atomic_store(&mode,fail);PVZSDLAppDelegate*e=[PVZSDLAppDelegate new];[e postFinishLaunch];until(^BOOL{return e.pvzRunning;});assert(mainCalls==1&&!PVZTakePreparedAudio()&&atomic_load(&audioRefs)==0&&atomic_load(&sdlRefs)==0);assert(atomic_load(&quitCalls)==(fail==2?1:0));}
 puts("PASS: actual audio queue/start/cancel/transfer functions; duplicate requests, main-loop nonblocking, inactive readiness, disconnect/reconnect during preparation, orphan cleanup, one-time ownership and both failure paths; cleanup-before-reopen asserted; balanced device/subsystem references");
}return 0;}
'''
p=Path('/tmp/pvz-audio-lifecycle-regression.m');p.write_text(pre+queue+'\n'+release+'\n'+take+classes+finish+'\n'+cancel+'\n'+post+tests)
subprocess.run(['xcrun','clang','-fobjc-arc','-g','-fsanitize=address,undefined','-framework','Foundation',str(p),'-o','/tmp/pvz-audio-lifecycle-regression'],check=True)
subprocess.run(['/tmp/pvz-audio-lifecycle-regression'],check=True,timeout=20)
