"""Check actual menu/confirmation fragments and actual main cleanup on iOS/Mac."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
selector = (root / 'src/Lawn/Widget/GameSelector.cpp').read_text()
app = (root / 'src/LawnApp.cpp').read_text()
entry = (root / 'src/main.cpp').read_text()


def block(text, start):
    begin = text.index('{', start)
    end, depth = begin + 1, 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


init = selector[selector.index('\tmQuitButton->mClip ='):selector.index('\n\tmChangeUserButton =')]
enable = '\n'.join(line for line in selector.splitlines()
                   if 'mQuitButton->mBtnNoDraw = false;' in line or 'mQuitButton->mMouseVisible = true;' in line)
draw_at = selector.index('\t\t\tmQuitButton->mButtonImage,')
draw_start = selector.rfind('g->DrawImageF(', 0, draw_at)
draw = selector[draw_start:selector.index(');', draw_at) + 2]
escape = block(selector, selector.index('if (theKey == KeyCode::KEYCODE_ESCAPE)'))
quit_start = selector.index('\tcase GameSelector::GameSelector_Quit:')
quit_case = selector[quit_start:selector.index('\tcase GameSelector::GameSelector_Help:', quit_start)]
confirm_start = app.index('\t\tcase Dialogs::DIALOG_QUIT:', app.index('void LawnApp::ButtonDepress('))
confirm_case = app[confirm_start:app.index('\t\tcase Dialogs::DIALOG_NAG:', confirm_start)]
close = block(app, app.index('void LawnApp::CloseRequestAsync()'))
cancel_start = app.index('if (theId % 10000 >= 3000', app.index('void LawnApp::ButtonDepress('))
cancel_block = block(app, cancel_start)
cancel_condition = cancel_block[:cancel_block.index('{')]
cancel_default = cancel_block[cancel_block.rindex('default:'):cancel_block.index('\n\t\t}', cancel_block.rindex('default:'))]

ui = r'''
#include <cassert>
#include <atomic>
#include <cstdio>
enum class KeyCode{KEYCODE_ESCAPE};
namespace Dialogs{constexpr int DIALOG_QUIT=4;}
struct Button{bool mClip=true,mBtnNoDraw=false,mMouseVisible=true,visible=true,disabled=false;int mButtonOffsetX=0,mButtonOffsetY=0,mX=0,mY=0,mButtonImage=1;void SetVisible(bool v){visible=v;}void SetDisabled(bool v){disabled=v;}};
struct LawnApp{int confirms=0;bool dialog=false,mExitToTop=false;std::atomic<bool>mCloseRequest=false;
 void ConfirmQuit(){++confirms;dialog=true;}void KillDialog(int){dialog=false;}
 void Respond(int);void CloseRequestAsync();};
struct Graphics{int draws=0;void DrawImageF(int,float,float){++draws;}};
struct GameSelector{enum{GameSelector_Quit=1};Button button;Button*mQuitButton=&button;LawnApp app;LawnApp*mApp=&app;
 void Init();void Open();void Draw(Graphics*);void KeyDown(KeyCode);void ButtonDepress(int);};
'''
ui += 'void GameSelector::Init(){' + init + '}\nvoid GameSelector::Open(){' + enable + '}\n'
ui += 'void GameSelector::Draw(Graphics*g){float aFractionalOffsetX=0,aFractionalOffsetY=0;' + draw + '}\n'
ui += 'void GameSelector::KeyDown(KeyCode theKey){' + escape + '}\n'
ui += 'void GameSelector::ButtonDepress(int id){switch(id){' + quit_case + '}}\n'
ui += close + '\nvoid LawnApp::Respond(int theId){if(theId>=2000&&theId<3000){switch(theId-2000){' + confirm_case + '}}'
ui += cancel_condition + '{switch(theId-3000){' + cancel_default + '}}}\n'
ui += r'''
int main(){GameSelector s;Graphics g;s.Init();s.Open();s.Draw(&g);
 assert(s.button.visible&&!s.button.disabled&&s.button.mMouseVisible&&!s.button.mBtnNoDraw&&g.draws==1);
 s.KeyDown(KeyCode::KEYCODE_ESCAPE);assert(s.app.dialog&&!s.app.mCloseRequest);
 s.app.Respond(Dialogs::DIALOG_QUIT+3000);assert(!s.app.dialog&&!s.app.mCloseRequest);
 s.ButtonDepress(GameSelector::GameSelector_Quit);assert(s.app.confirms==2&&s.app.dialog);
 s.app.Respond(Dialogs::DIALOG_QUIT+2000);assert(!s.app.dialog&&s.app.mCloseRequest&&s.app.mExitToTop);
 std::puts("PASS actual quit UI and response: visible/drawable/actionable, cancel keeps running, confirm requests orderly shutdown");}
'''
ui = ui.replace('{#', '{\n#').replace(';#', ';\n#').replace('#endif}', '#endif\n}')

main_body = block(entry, entry.index('int main(')).replace('int main(', 'int GameEntry(', 1)
cleanup = r'''
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <string_view>
using namespace std;
bool requested=false,stopped=false,destroyed=false;
namespace Sexy{int ExtractResourcesByName=1;}
int gExtractResourcesByName=0,gLawnStringFormats=0,gLawnStringFormatCount=0;
void PvzpStringListSetColors(int,int){}
struct LawnApp{atomic<bool>mCloseRequest=requested;
 void SetArgs(int,char**){}void Init(){}void Start(){}
 void Shutdown(){stopped=true;}~LawnApp(){assert(stopped);destroyed=true;}};
LawnApp*gLawnApp;
void AtExit(){assert(stopped&&destroyed);std::puts("CLEANUP_COMPLETE");}
'''
cleanup += main_body + r'''
int main(int argc,char**argv){requested=argc>1;std::atexit(AtExit);assert(GameEntry(argc,argv)==0);
 assert(stopped&&destroyed);std::puts("ENTRY_RETURNED");}
'''

with tempfile.TemporaryDirectory(prefix='pvz-quit-') as directory:
    directory = Path(directory)
    for ios in [False, True]:
        platform = 'iOS' if ios else 'Mac'
        for name, code in [('ui', ui), ('cleanup', cleanup)]:
            cpp, binary = directory / (name + '.cpp'), directory / name
            cpp.write_text(code)
            cmd = ['xcrun', 'clang++', '-std=c++20', '-fsanitize=address,undefined', '-Werror']
            if ios:
                cmd += ['-D__IPHONEOS__=1']
            subprocess.run(cmd + [str(cpp), '-o', str(binary)], check=True)
            if name == 'ui':
                subprocess.run([str(binary)], check=True)
            else:
                for quit_requested in [False, True]:
                    result = subprocess.run([str(binary)] + (['quit'] if quit_requested else []),
                                            check=True, text=True, capture_output=True)
                    assert 'CLEANUP_COMPLETE' in result.stdout
                    assert ('ENTRY_RETURNED' in result.stdout) == (not ios or not quit_requested)
        print(f'PASS {platform}: actual entry completes shutdown/destruction before completion; explicit iOS quit exits with status 0', flush=True)
