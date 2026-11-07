import sys
from pathlib import Path
import subprocess
r=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else Path(__file__).resolve().parents[1];s=(r/'src/LawnApp.cpp').read_text()
def block(text,a):
 b=text.index('{',a);i=b+1;d=1
 while d:d+=(text[i]=='{')-(text[i]=='}');i+=1
 return text[a:i]
confirm=block(s,s.index('void LawnApp::ConfirmQuit()'))
case_start=s.index('\t\tcase Dialogs::DIALOG_QUIT:');case_end=s.index('\t\tcase Dialogs::DIALOG_NAG:',case_start);quit_case=s[case_start:case_end]
no=block(s,s.index('if (theId % 10000 >= 3000'))
pre=r'''#define __IPHONEOS__ 1
#include <string>
#include <cassert>
#include <iostream>
#include "ConstEnums.h"
std::string PvzpStringTranslate(const std::string&s){return "translated:"+s;}
struct Dialog {enum{BUTTONS_OK_CANCEL=2};};
struct Button{std::string mLabel;};
struct LawnDialog{Button yes;Button*mLawnYesButton=&yes;int mWidth=300,mHeight=200;};
struct LawnApp{
 LawnDialog dialog;bool dialogOpen=false,alive=true;int saves=0,score=123,scene=77,shutdowns=0,closeRequests=0;std::string header,body;
 void ConfirmQuit();void Press(int);
 LawnDialog*DoDialog(Dialogs,bool,const std::string&h,const std::string&b,const std::string&,int){header=h;body=b;dialogOpen=true;return &dialog;}
 void CenterDialog(LawnDialog*,int,int){}
 void KillDialog(int){dialogOpen=false;}
 void CloseRequestAsync(){++closeRequests;alive=false;}
 void Shutdown(){++shutdowns;alive=false;}
 void FinishUserDialog(bool){}void FinishCreateUserDialog(bool){}void FinishConfirmDeleteUserDialog(bool){}void FinishRenameUserDialog(bool){}void FinishCheatDialog(bool){}void FinishTimesUpDialog(){}
};
'''
press='void LawnApp::Press(int theId){if (theId % 10000 >= 2000 && theId % 10000 < 3000){switch(theId-2000){'+quit_case+'default:return;}}'+no+'}'
post=r'''
int main(){LawnApp app;
 app.ConfirmQuit();assert(app.dialogOpen&&app.header=="translated:[QUIT_HEADER]"&&app.body=="translated:[QUIT_MESSAGE]"&&app.dialog.yes.mLabel=="translated:[QUIT_BUTTON]");
 app.Press(3000+Dialogs::DIALOG_QUIT);assert(!app.dialogOpen&&app.saves==0&&app.alive&&app.score==123&&app.scene==77&&app.shutdowns==0&&app.closeRequests==0);
 app.ConfirmQuit();app.Press(2000+Dialogs::DIALOG_QUIT);assert(!app.dialogOpen&&app.saves==0&&app.alive&&app.score==123&&app.scene==77&&app.shutdowns==0&&app.closeRequests==0);
 for(int i=0;i<100;i++){app.ConfirmQuit();app.Press((i%2?2000:3000)+Dialogs::DIALOG_QUIT);assert(app.saves==0&&app.alive&&app.score==123&&app.scene==77);}
 std::cout<<"PASS: restored actual ConfirmQuit, actual iOS confirm case and cancel dispatch; original text preserved; confirm/cancel perform no save, shutdown or scene/score change (legacy behavior pending product decision)\n";
}
'''
p=Path('/tmp/pvz-original-exit-regression.cpp');p.write_text(pre+confirm+press+post)
subprocess.run(['xcrun','clang++','-std=c++20','-O1','-g','-fsanitize=address,undefined','-Wshorten-64-to-32','-Werror','-I',str(r/'src'),str(p),'-o','/tmp/pvz-original-exit-regression'],check=True)
subprocess.run(['/tmp/pvz-original-exit-regression'],check=True,timeout=15)
