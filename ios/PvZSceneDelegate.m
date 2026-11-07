// SPDX-License-Identifier: LGPL-3.0-or-later
// Single-scene UIKit bridge for SDL 2's legacy application delegate.
#import <UIKit/UIKit.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include <SDL_mixer_ext/SDL_mixer_ext.h>

// SDL documents subclassing this delegate via getAppDelegateClassName.
// Keep its implementation in the existing SDL library, not a copied backend.
@interface SDLUIKitDelegate : UIResponder <UIApplicationDelegate>
- (void)postFinishLaunch;
@end

static __weak UIWindowScene *PVZWindowScene;
static __weak UIWindow *PVZGameWindow;
static BOOL PVZSceneActive;
static Uint64 PVZInputEpoch;
static Uint32 PVZInputResetTicks;
static BOOL PVZPreparedAudio;
static unsigned int PVZSceneLogBudget = 64;

static dispatch_queue_t PVZAudioQueue(void)
{
    static dispatch_queue_t queue;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ queue = dispatch_queue_create("io.github.wszqkzqk.pvz.audio", DISPATCH_QUEUE_SERIAL); });
    return queue;
}

static void PVZReleasePreparedAudio(BOOL ready, dispatch_block_t completion)
{
    dispatch_async(PVZAudioQueue(), ^{
        if (ready) {
            Mix_CloseAudio();
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
        }
        if (completion) dispatch_async(dispatch_get_main_queue(), completion);
    });
}

int PVZIsSceneActive(void) { return PVZSceneActive; }
Uint64 PVZSceneInputEpoch(void) { return PVZInputEpoch; }
Uint32 PVZSceneInputResetTicks(void) { return PVZInputResetTicks; }

// Audio ownership transfers once to SDLSoundManager; no second open on main.
int PVZTakePreparedAudio(void)
{
    BOOL available = PVZPreparedAudio;
    PVZPreparedAudio = NO;
    return available;
}

static void PVZNoteSceneState(BOOL active)
{
    PVZSceneActive = active;
    ++PVZInputEpoch;
    PVZInputResetTicks = PVZGameWindow ? SDL_GetTicks() : 0;
    if (PVZSceneLogBudget > 0) {
        --PVZSceneLogBudget;
        NSLog(@"PVZScene[ta-r4]: unix=%.3f tick=%u active=%d input epoch=%llu", NSDate.date.timeIntervalSince1970,
              PVZInputResetTicks, active, (unsigned long long)PVZInputEpoch);
    }
}

@interface PVZSceneDelegate : UIResponder <UIWindowSceneDelegate>
@property(nonatomic, strong) UIWindow *window;
@end

@interface PVZSDLAppDelegate : SDLUIKitDelegate
@property(nonatomic, assign) BOOL pvzStarted;
@property(nonatomic, assign) BOOL pvzAudioReady;
@property(nonatomic, assign) BOOL pvzRunning;
@property(nonatomic, assign) NSUInteger pvzAudioGeneration;
- (void)cancelPendingStart;
- (void)finishCancelledPreparation:(BOOL)ready;
@end

@implementation SDLUIKitDelegate (PVZSceneApplication)
+ (NSString *)getAppDelegateClassName
{
    return NSStringFromClass(PVZSDLAppDelegate.class);
}
@end

static void SDLCALL PVZIdleTimerChanged(void *context, const char *name,
                                      const char *oldValue, const char *value)
{
    (void)context;
    (void)name;
    (void)oldValue;
    UIApplication.sharedApplication.idleTimerDisabled = value && *value != '0';
}

@implementation PVZSDLAppDelegate
- (void)finishCancelledPreparation:(BOOL)ready
{
    // Keep pvzStarted set until cleanup completes: no new preparation or game
    // startup can race an older lease's Mix_CloseAudio/SDL_QuitSubSystem.
    PVZReleasePreparedAudio(ready, ^{
        self.pvzStarted = NO;
        self.pvzAudioReady = NO;
        [self performSelector:@selector(postFinishLaunch) withObject:nil afterDelay:0.0];
    });
}

- (void)cancelPendingStart
{
    if (self.pvzRunning || !self.pvzStarted) return;
    ++self.pvzAudioGeneration;
    if (!self.pvzAudioReady) return; // The in-flight completion owns cleanup.
    self.pvzAudioReady = NO;
    BOOL ready = PVZPreparedAudio;
    PVZPreparedAudio = NO;
    [self finishCancelledPreparation:ready];
}

- (BOOL)application:(UIApplication *)application
        didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    (void)application;
    (void)launchOptions;
    // UIKit owns the launch storyboard. Avoid SDL's legacy, scene-less window.
    NSLog(@"PVZBuild: ios-keyboard-r8-visible-controls-20261031");
    [NSFileManager.defaultManager changeCurrentDirectoryPath:NSBundle.mainBundle.resourcePath];
    SDL_AddHintCallback(SDL_HINT_IDLE_TIMER_DISABLED, PVZIdleTimerChanged, NULL);
    SDL_SetMainReady();
    return YES;
}

- (void)postFinishLaunch
{
    if (self.pvzRunning || !PVZWindowScene || !PVZSceneActive) {
        return;
    }
    if (!self.pvzStarted) {
        self.pvzStarted = YES;
        // AVAudioSession activation may block. Keep UIKit's main run loop free
        // until the playback device is ready; do not synchronously wait on it.
        const NSUInteger generation = ++self.pvzAudioGeneration;
        dispatch_async(PVZAudioQueue(), ^{
            BOOL ready = NO;
            if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) {
                ready = Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 2048) == 0;
                if (!ready) SDL_QuitSubSystem(SDL_INIT_AUDIO);
            }
            if (!ready) NSLog(@"PVZAudio: preparation failed: %s", SDL_GetError());
            dispatch_async(dispatch_get_main_queue(), ^{
                if (generation != self.pvzAudioGeneration || !PVZWindowScene) {
                    [self finishCancelledPreparation:ready];
                    return;
                }
                PVZPreparedAudio = ready;
                self.pvzAudioReady = YES;
                // SDL_main pumps a nested run loop for the game's lifetime.
                // Leave this GCD main-queue block before starting it, otherwise
                // UIKit's later main-queue work (including keyboard display)
                // cannot run. Keep SDL's original run-loop selector entry.
                [self performSelector:@selector(postFinishLaunch) withObject:nil afterDelay:0.0];
            });
        });
        return;
    }
    if (!self.pvzAudioReady || !PVZSceneActive) return;
    self.pvzRunning = YES;
    NSLog(@"PVZScene: starting SDL main after scene/audio readiness");
    dispatch_async(dispatch_get_main_queue(), ^{
        NSLog(@"PVZStartup[r7]: main-queue probe serviced after SDL main entry");
    });
    [super postFinishLaunch];
}
@end

@implementation PVZSceneDelegate
- (void)scene:(UIScene *)scene
        willConnectToSession:(UISceneSession *)session
        options:(UISceneConnectionOptions *)connectionOptions
{
    (void)session;
    (void)connectionOptions;
    if (![scene isKindOfClass:UIWindowScene.class]) {
        return;
    }
    PVZWindowScene = (UIWindowScene *)scene;
    if (PVZGameWindow) {
        PVZGameWindow.windowScene = PVZWindowScene;
        self.window = PVZGameWindow;
        [self.window makeKeyAndVisible];
    }
    NSLog(@"PVZScene: connected application window scene");
    // Return from UIKit's connection callback before entering SDL's event loop.
    [(PVZSDLAppDelegate *)UIApplication.sharedApplication.delegate
        performSelector:@selector(postFinishLaunch) withObject:nil afterDelay:0.0];
}

- (void)sceneDidBecomeActive:(UIScene *)scene
{
    if (scene != PVZWindowScene) return;
    PVZNoteSceneState(YES);
    [self.window makeKeyAndVisible];
    [(PVZSDLAppDelegate *)UIApplication.sharedApplication.delegate
        performSelector:@selector(postFinishLaunch) withObject:nil afterDelay:0.0];
}

- (void)sceneWillResignActive:(UIScene *)scene
{
    if (scene == PVZWindowScene) PVZNoteSceneState(NO);
}

- (void)sceneDidEnterBackground:(UIScene *)scene
{
    if (scene == PVZWindowScene) PVZNoteSceneState(NO);
}

- (void)sceneWillEnterForeground:(UIScene *)scene
{
    if (scene == PVZWindowScene) PVZNoteSceneState(NO);
}

- (void)sceneDidDisconnect:(UIScene *)scene
{
    if (scene == PVZWindowScene) {
        PVZNoteSceneState(NO);
        [(PVZSDLAppDelegate *)UIApplication.sharedApplication.delegate cancelPendingStart];
        PVZWindowScene = nil;
    }
    self.window = nil;
}
@end

int PVZAttachSDLWindowToScene(SDL_Window *window)
{
    SDL_SysWMinfo information;
    SDL_VERSION(&information.version);
    if (!SDL_GetWindowWMInfo(window, &information)) {
        return 0;
    }
    if (information.subsystem != SDL_SYSWM_UIKIT || !PVZWindowScene) {
        return SDL_SetError("PvZ has no connected UIKit window scene"), 0;
    }
    UIWindow *uiwindow = information.info.uikit.window;
    uiwindow.windowScene = PVZWindowScene;
    PVZGameWindow = uiwindow;
    ((PVZSceneDelegate *)PVZWindowScene.delegate).window = uiwindow;
    [uiwindow makeKeyAndVisible];
    NSLog(@"PVZScene: SDL window attached to its scene");
    return 1;
}
