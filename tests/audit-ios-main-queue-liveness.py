"""Check that the actual startup method leaves GCD main work runnable in SDL's loop."""
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'source/ios/PvZSceneDelegate.m').read_text()


def extract(name, start=0):
    begin = source.index(name, start)
    cursor = source.index('{', begin) + 1
    depth = 1
    while depth:
        depth += (source[cursor] == '{') - (source[cursor] == '}')
        cursor += 1
    return source[begin:cursor]


queue = extract('static dispatch_queue_t PVZAudioQueue(')
post = extract('- (void)postFinishLaunch', source.index('@implementation PVZSDLAppDelegate'))
fixture = r'''
#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>
#include <assert.h>
#include <string.h>
static id PVZWindowScene;
static BOOL PVZSceneActive, PVZPreparedAudio;
static BOOL mainProbe, timerProbe, mainInside, timerInside, loopFinished;
static int mainCalls;
#define SDL_INIT_AUDIO 1
#define AUDIO_S16SYS 1
static int SDL_InitSubSystem(int flags) { assert(!NSThread.isMainThread); return 0; }
static int Mix_OpenAudio(int f, int format, int channels, int buffer) { assert(!NSThread.isMainThread); return 0; }
static void SDL_QuitSubSystem(int flags) { assert(!NSThread.isMainThread); }
static const char *SDL_GetError(void) { return "test"; }
__QUEUE__
@interface FakeSDLDelegate : NSObject
- (void)postFinishLaunch;
- (void)timerProbe;
@end
@implementation FakeSDLDelegate
- (void)timerProbe { timerProbe=YES; }
- (void)postFinishLaunch {
    assert(NSThread.isMainThread); ++mainCalls;
    dispatch_async(dispatch_get_main_queue(), ^{ mainProbe=YES; });
    [self performSelector:@selector(timerProbe) withObject:nil afterDelay:.005];
    // Model SDL_main's long-lived loop pumping UIKit's default run-loop mode.
    NSDate *limit=[NSDate dateWithTimeIntervalSinceNow:.15];
    while (limit.timeIntervalSinceNow>0) {
        [NSRunLoop.currentRunLoop runMode:NSDefaultRunLoopMode
                              beforeDate:[NSDate dateWithTimeIntervalSinceNow:.001]];
    }
    mainInside=mainProbe; timerInside=timerProbe; loopFinished=YES;
}
@end
@interface PVZSDLAppDelegate : FakeSDLDelegate
@property BOOL pvzStarted, pvzAudioReady, pvzRunning;
@property NSUInteger pvzAudioGeneration;
- (void)finishCancelledPreparation:(BOOL)ready;
@end
@implementation PVZSDLAppDelegate
- (void)finishCancelledPreparation:(BOOL)ready { assert(!"unexpected cancellation in startup liveness test"); }
__POST__
@end
int main(int argc, char **argv) { @autoreleasepool {
    BOOL expectStarved=argc>1 && !strcmp(argv[1],"--expect-starved");
    PVZWindowScene=[NSObject new]; PVZSceneActive=YES;
    PVZSDLAppDelegate *app=[PVZSDLAppDelegate new]; [app postFinishLaunch];
    NSDate *limit=[NSDate dateWithTimeIntervalSinceNow:3];
    while ((!loopFinished || !mainProbe) && limit.timeIntervalSinceNow>0) {
        [NSRunLoop.currentRunLoop runMode:NSDefaultRunLoopMode
                              beforeDate:[NSDate dateWithTimeIntervalSinceNow:.002]];
    }
    assert(loopFinished && mainCalls==1 && timerInside && mainProbe);
    printf("actual startup: run-loop timer inside SDL loop=%d; GCD main work inside SDL loop=%d; GCD probe eventually executed=%d\n",
           timerInside, mainInside, mainProbe);
    assert(mainInside == !expectStarved);
    puts(expectStarved ? "REPRODUCED: actual startup holds GCD main queue throughout SDL loop while run-loop events still run"
                       : "PASS: actual startup lets GCD main work and run-loop events run before SDL main returns; ASan/UBSan");
} }
'''.replace('__QUEUE__', queue).replace('__POST__', post)
with tempfile.TemporaryDirectory(prefix='pvz-main-queue-') as directory:
    path = Path(directory) / 'test.m'
    binary = Path(directory) / 'test'
    path.write_text(fixture)
    subprocess.run(['xcrun', 'clang', '-fobjc-arc', '-fsanitize=address,undefined',
                    '-framework', 'Foundation', str(path), '-o', str(binary)], check=True)
    subprocess.run([str(binary), *sys.argv[1:]], check=True, timeout=10)
