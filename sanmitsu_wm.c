

#include <X11/Xlib.h>

#define MAX(A,B) ((A) > (B) ? (A) : (B))

Display * snmt_display;     // ディスプレイ
Window    snmt_root_window; // 根ウインドウ
XEvent    snmt_event;       // イベントの受け皿

const unsigned int titlebar_width_margin = 25; // タイトルバーの右端の開幅  = Exitボタンの幅
const unsigned int titlebar_height       = 25; // タイトルバーの高さ       = Exitボタンの高さ

// 色の取得
unsigned long snmt_color(char * _color) {

    XColor near_color, true_color;
    XAllocNamedColor(snmt_display,
                     DefaultColormap(snmt_display, 0), _color,
                     &near_color, &true_color);

    return BlackPixel(snmt_display,0)^near_color.pixel;

}

// 新しいウインドウにboxやexitを追加する関数。
Bool snmt_box_new_window(){

    Window
        group_app = snmt_event.xmap.window,
        group_box,
        group_exit;

    // appのAttributes取得
    XWindowAttributes targ_attr;
    XGetWindowAttributes(snmt_display, group_app, &targ_attr);

    // boxのAttributes取得
    XSetWindowAttributes box_exit_attr;
    box_exit_attr.override_redirect = True;

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
        &box_exit_attr
    );

    // boxの詳細設定
    XDefineCursor(snmt_display, group_box, XCreateFontCursor(snmt_display, 90));
    XSetWindowBackground(snmt_display, group_box, snmt_color("orange"));
    XReparentWindow(snmt_display, group_app, group_box, 0, titlebar_height);
    XMapWindow(snmt_display, group_box);
    XSelectInput(snmt_display, group_box, SubstructureNotifyMask);
    
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
        &box_exit_attr
    );

    // exitの詳細設定
    XDefineCursor(snmt_display, group_exit, XCreateFontCursor(snmt_display, 90));
    XSetWindowBackground(snmt_display, group_exit, snmt_color("red"));
    XMapWindow(snmt_display, group_exit);
    XSelectInput(snmt_display, group_exit, None);

    return True;
}


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

    // 除去されるウインドウを格納。DestroyNotifyの項にて利用。
    Window destroy_target_window = None;

    // メインループ
    while(1){
        // イベントの取得
        XNextEvent(snmt_display, &snmt_event);

        switch (snmt_event.type){

            // ウインドウが新しくマッピングされた時
            case MapNotify:

                // override_redirectな奴に用はない。
                // boxウインドウやexitウインドウを作成する関数:snmt_box_new_window()を呼び出す。
                if(snmt_event.xmap.override_redirect == False)
                    snmt_box_new_window();

                break;

            // ウインドウが除去された時
            case DestroyNotify:
            {
                // 除去対象である(されない可能性もある)ウインドウ。以下の処理で、boxウインドウであるかを確認し、除去判定をする。
                const Window destroy_request_window = snmt_event.xdestroywindow.event;

                //　除去対象がない？？信じられない、死になさい(無慈悲)!!!!!
                if(destroy_request_window == None) break;

                // DestroyNotifyが"最初に"呼び出される場合、そのウインドウはboxウインドウである。
                // 逆に最初ではない場合(box自体が除去される時のNotifyなど)は、以下の条件にて無視することになる。
                if(destroy_request_window == destroy_target_window){
                    destroy_target_window = None;
                    break;
                }

                // boxウインドウを除去。
                XDestroyWindow(snmt_display, destroy_request_window);
                destroy_target_window = destroy_request_window;

            }
                break;


            // ボタンが押された時
            case ButtonPress:
                grip_info = snmt_event.xbutton;
                if(grip_info.subwindow == None) break;
                XGetWindowAttributes(snmt_display, grip_info.subwindow, &grip_attributes);
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
}

