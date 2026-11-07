"""Exercise the real edit/username geometry methods with controlled fonts and layout."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]


def method(path, name):
    text = (root / path).read_text()
    start = text.index(name)
    cursor = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[cursor] == '{') - (text[cursor] == '}')
        cursor += 1
    return text[start:cursor]


edit = method('source/src/SexyAppFramework/widget/EditWidget.cpp', 'void EditWidget::UpdateTextInputArea()')
resize = method('source/src/Lawn/Widget/NewUserDialog.cpp', 'void NewUserDialog::Resize(')
fixture = r'''
#include <algorithm>
#include <cassert>
#include <cstdio>
#define __IPHONEOS__ 1
struct Rect { int mX,mY,mWidth,mHeight; Rect(int x=0,int y=0,int w=0,int h=0):mX(x),mY(y),mWidth(w),mHeight(h){} };
struct Point { int mX,mY; };
struct Font { int height=18; int GetHeight(){return height;} };
struct App { Rect region; int calls=0; void SetTextInputRect(Rect r){region=r;++calls;} };
struct WidgetManager { App *mApp; };
struct EditWidget {
    Font *mFont=nullptr; WidgetManager *mWidgetManager=nullptr; bool mHasFocus=false;
    int mX=0,mY=0,mWidth=200,mHeight=28,mTextInputBottomPadding=0,caret=0;
    Point parent={0,0};
    Point GetAbsPos(){return {parent.mX+mX,parent.mY+mY};}
    int GetCaretXOffset(){return caret;}
    void Resize(int x,int y,int w,int h){mX=x;mY=y;mWidth=w;mHeight=h;}
    void UpdateTextInputArea();
};
struct Button {int mY,mHeight;};
struct Insets {int mLeft=36,mRight=46;};
struct LawnDialog {
    int mHeight=0,mWidth=0; Insets mContentInsets; Button yes,no;
    Button *mLawnYesButton=&yes,*mLawnNoButton=&no;
    void Resize(int x,int y,int w,int h){mWidth=w;mHeight=h;yes={h-90,40};no={h-90,40};}
};
struct NewUserDialog:LawnDialog {EditWidget field;EditWidget *mNameEditWidget=&field;void Resize(int,int,int,int);};
__EDIT__
__RESIZE__
int main(){
    App app;WidgetManager manager{&app};Font font;NewUserDialog dialog;
    EditWidget &edit=dialog.field;edit.mFont=&font;edit.mWidgetManager=&manager;edit.mHasFocus=true;
    // Vary dialog origin, size and font. The reserved region must contain the
    // action buttons with an eight-unit gap, independent of caret height.
    for(int height:{280,300,340})for(int textHeight:{18,24,42}){
        dialog.Resize(200,100,400,height);edit.parent={200,100};font.height=textHeight;
        edit.caret=900;edit.UpdateTextInputArea();
        const Rect &r=app.region;
        assert(r.mX==200+edit.mX+4+edit.mWidth-8);
        assert(r.mY+r.mHeight==100+dialog.no.mY+dialog.no.mHeight+8);
        assert(r.mY<=100+edit.mY+edit.mHeight);
        assert(r.mY+r.mHeight>=100+edit.mY+edit.mHeight);
    }
    // Unconfigured fields retain their exact caret rect.
    edit.mTextInputBottomPadding=0;edit.caret=0;font.height=18;edit.UpdateTextInputArea();
    assert(app.region.mHeight==18 && app.region.mWidth==1);
    int calls=app.calls;edit.mHasFocus=false;edit.UpdateTextInputArea();assert(app.calls==calls);
    edit.mHasFocus=true;edit.mFont=nullptr;edit.UpdateTextInputArea();assert(app.calls==calls);
    edit.mFont=&font;edit.mWidgetManager=nullptr;edit.UpdateTextInputArea();assert(app.calls==calls);
    puts("PASS actual EditWidget input-region and NewUserDialog resize: both buttons and field reserved with gap across sizes/fonts; default caret behavior unchanged; focus/attachment guards; ASan/UBSan");
}
'''.replace('__EDIT__', edit).replace('__RESIZE__', resize)
with tempfile.TemporaryDirectory(prefix='pvz-input-region-') as directory:
    path = Path(directory) / 'test.cpp'
    binary = Path(directory) / 'test'
    path.write_text(fixture)
    subprocess.run(['xcrun', 'clang++', '-std=c++20', '-fsanitize=address,undefined',
                    str(path), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True, timeout=10)
