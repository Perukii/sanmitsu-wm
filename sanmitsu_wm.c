

#include <X11/Xlib.h>

#define MAX(A,B) ((A) > (B) ? (A) : (B))

Display * snmt_display;     // ディスプレイ
Window    snmt_root_window; // 根ウインドウ
XEvent    snmt_event;       // イベントの受け皿

const unsigned int titlebar_width_margin = 25; // タイトルバーの右端の開幅  = Exitボタンの幅
const unsigned int titlebar_height       = 25; // タイトルバーの高さ       = Exitボタンの高さ

// 色の取得を行う関数。
unsigned long snmt_color(const char * _color);

// ウインドウがboxかを調べる関数。
Bool snmt_window_is_box(const Window);

// 新しいウインドウにboxやexitを追加する関数。
Bool snmt_box_new_window();

// boxウインドウの消去を試みる関数。
Bool snmt_delete_window(const Window, Window *);

// boxウインドウのリサイズを行う関数。
Bool snmt_resize();

int main(){

    // Xサーバに接続
    snmt_display = XOpenDisplay(0);
    if(snmt_display == NULL) return 0;

    // 根ウインドウ
    snmt_root_window = RootWindow(snmt_display, 0);

    // 根ウインドウに渡されるイベント
    XSelectInput(snmt_display, snmt_root_window,
                SubstructureNotifyMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask);

    // ボタン入力
    XGrabButton(snmt_display,
                1, Mod1Mask, snmt_root_window,
                True,
                ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                GrabModeAsync, GrabModeAsync, None, None);

    XGrabButton(snmt_display,
                3, Mod1Mask,
                snmt_root_window, True,
                ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                GrabModeAsync, GrabModeAsync, None, None);

    // gripされているウインドウの情報を格納する構造体の定義・初期化
    XWindowAttributes grip_attributes;
    XButtonEvent grip_info;

    grip_info.button    = None;
    grip_info.x_root    = None;
    grip_info.y_root    = None;
    grip_info.subwindow = None;

    //===

    // 最後に除去されたウインドウを格納。DestroyNotifyの項にて利用。
    Window last_destroyed_window = None;

    // メインループ
    while(1){
        // イベントの取得
        XNextEvent(snmt_display, &snmt_event);

        switch (snmt_event.type){

            // ウインドウが新しくマップされた時
            case MapNotify:                
                // boxウインドウやexitウインドウを作成する関数。
                snmt_box_new_window();
                break;

            // ウインドウが除去された時
            case DestroyNotify:
                // 以下の関数で、除去対象がboxウインドウであるかを確認し、除去判定をする。
                snmt_delete_window(snmt_event.xdestroywindow.event, &last_destroyed_window);
                break;

            // ウインドウが移動/拡縮された時
            case ConfigureNotify:
                snmt_resize();    
                break;

            // ボタンが押された時
            case ButtonPress:
                grip_info = snmt_event.xbutton;

                if(grip_info.subwindow != None && snmt_window_is_box(grip_info.subwindow)){
                    XRaiseWindow(snmt_display, grip_info.subwindow);
                    XGetWindowAttributes(snmt_display, grip_info.subwindow, &grip_attributes);
                }
                else if(grip_info.window != None){
                    XDestroyWindow(snmt_display, grip_info.window);
                    //snmt_delete_window(grip_info.window, &last_destroyed_window);
                }

                break;
            
            // ボタンが離された時
            case ButtonRelease:
                grip_info.subwindow = None;
                break;

            // カーソルが移動した時 
            case MotionNotify:

                // 掴まれているウインドウを持っていないなら、退出する。
                if(grip_info.subwindow == None) break;

                // 動く距離
                struct { unsigned int x, y; } move_to = {
                    snmt_event.xbutton.x_root - grip_info.x_root,
                    snmt_event.xbutton.y_root - grip_info.y_root
                };

                // ボタン1が押されたら移動、それ以外なら拡縮する。
                if(grip_info.button == 1){
                    XMoveWindow(snmt_display, grip_info.subwindow,
                                grip_attributes.x + move_to.x,
                                grip_attributes.y + move_to.y);
                }
                else{
                    XResizeWindow(snmt_display, grip_info.subwindow,
                                    MAX(grip_attributes.width  + move_to.x, 1),
                                    MAX(grip_attributes.height + move_to.y, 1));
                }
                
                break;

            default:
                break;
        }
    }
    return 1;
}

// 色の取得を行う関数。
unsigned long snmt_color(const char * _color) {

    XColor near_color, true_color;
    XAllocNamedColor(snmt_display,
                     DefaultColormap(snmt_display, 0), _color,
                     &near_color, &true_color);

    return BlackPixel(snmt_display,0)^near_color.pixel;

}

// ウインドウがboxかを調べる関数。
inline Bool snmt_window_is_box(const Window _target){
    Window root, parent, * child;
    unsigned int nchild;
    XQueryTree(snmt_display, _target, &root, &parent, &child, &nchild);

    return root == parent;
}

// 新しいウインドウにboxやexitを追加する関数。
Bool snmt_box_new_window(){
    
    // override_redirectな奴に用はない。
    if(snmt_event.xmap.override_redirect == True) return False;

    Window
        group_app = snmt_event.xmap.window,
        group_box,
        group_exit;

    // appのAttributes取得
    XWindowAttributes targ_attr;
    XGetWindowAttributes(snmt_display, group_app, &targ_attr);

    // boxのAttributes設定
    XSetWindowAttributes box_attr;
    box_attr.override_redirect = True;

    // boxを作成
    group_box = XCreateWindow(
        snmt_display,
        snmt_root_window,
        targ_attr.x,
        targ_attr.y,
        targ_attr.width,
        targ_attr.height + titlebar_height,
        0,
        0,
        InputOutput,
        CopyFromParent,
        CWOverrideRedirect,
        &box_attr
    );

    // boxの詳細設定
    XDefineCursor(snmt_display, group_box, XCreateFontCursor(snmt_display, 90));
    XSetWindowBackground(snmt_display, group_box, snmt_color("orange"));
    XSetWindowBorderWidth(snmt_display, group_box, 3);
    XReparentWindow(snmt_display, group_app, group_box, 0, titlebar_height);
    XMapWindow(snmt_display, group_box);
    XSelectInput(snmt_display, group_box, SubstructureNotifyMask);
    
    XSetWindowAttributes exit_attr;
    exit_attr.override_redirect = True;
    
    
    // exitを作成
    group_exit = XCreateWindow(
        snmt_display,
        group_box,
        targ_attr.x + targ_attr.width - titlebar_width_margin,
        targ_attr.y,
        titlebar_width_margin,
        titlebar_height,
        0,
        0,
        InputOutput,
        CopyFromParent,
        CWOverrideRedirect,
        &exit_attr
    );

    // exitの詳細設定
    XDefineCursor(snmt_display, group_exit, XCreateFontCursor(snmt_display, 90));
    XSetWindowBackground(snmt_display, group_exit, snmt_color("red"));
    XMapWindow(snmt_display, group_exit);
    XSelectInput(snmt_display, group_exit, ButtonPressMask);

    XGrabButton(snmt_display,
                1, Mod1Mask, group_exit,
                True, ButtonPressMask,
                GrabModeAsync, GrabModeAsync, None, None);
                
    return True;
}

// boxウインドウの消去を試みる関数。
Bool snmt_delete_window(const Window _destroy_request_window, Window * _last_destroyed_window){

    //　除去対象がない？？信じられない、死になさい(無慈悲)!!!!!
    if(_destroy_request_window == None) return False;

    // 最後に除去されたウインドウと同じウインドウが除去されようとしている場合、退出。
    if(_destroy_request_window == *_last_destroyed_window) return False;

    // 除去対象が根ウインドウと直結されていた場合(親=根だった場合)、boxウインドウを除去。
    if(snmt_window_is_box(_destroy_request_window)){
        *_last_destroyed_window = _destroy_request_window;
        Window root, parent, * child;
        unsigned int nchild;
        XQueryTree(snmt_display, _destroy_request_window, &root, &parent, &child, &nchild);

        XEvent event;
        event.xclient.type = ClientMessage;
        event.xclient.message_type = XInternAtom(snmt_display, "WM_PROTOCOLS", True);
        event.xclient.format = 32;
        event.xclient.data.l[0] = XInternAtom(snmt_display, "WM_DELETE_WINDOW", True);
        event.xclient.data.l[1] = CurrentTime;

        for(unsigned int ci=0;ci<nchild;ci++){
            event.xclient.window = child[ci];
            XSendEvent(snmt_display, child[ci], False, NoEventMask, &event);
        }

        // unmapでごまかしているが、これはDestroyがうまくいかなかったため...許して！
        XUnmapWindow(snmt_display, _destroy_request_window);
        
    }
    return True;
}

// boxウインドウのリサイズを行う関数。
Bool snmt_resize(){

    if(snmt_event.xconfigure.window == None) return False;

    Window root, parent, * child;
    unsigned int nchild;
    XQueryTree(snmt_display, snmt_event.xconfigure.window, &root, &parent, &child, &nchild);

    for(unsigned int ci = 0; ci<nchild; ci++){

        XWindowAttributes child_attr;
        XGetWindowAttributes(snmt_display, child[ci], &child_attr);

        if(child_attr.override_redirect == False){
            XResizeWindow(snmt_display, child[ci],
                snmt_event.xconfigure.width,
                snmt_event.xconfigure.height - titlebar_height);
        }
        else{
            XMoveWindow(snmt_display, child[ci],
                snmt_event.xconfigure.width - titlebar_width_margin, 0);
        }
    }

    return True;
}