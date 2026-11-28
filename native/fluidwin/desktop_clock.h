#pragma once
#include "tlauncher_identity.h"
#include "testi.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <shellscalingapi.h>
#include <string>
#include <vector>
#include <windowsx.h>
#include <usp10.h>
#include <fstream>
#include <unordered_set>

namespace desktopclock {
struct Settings {
    bool enabled=false, locked=true, clickThrough=true, date=true, seconds=false;
    int style=0, scale=100, opacity=72;
    COLORREF color=RGB(185,231,245);
    std::wstring font, display;
    int x=-1,y=-1;
    bool operator==(const Settings&)const=default;
};
struct Monitor {
    HMONITOR handle=nullptr;
    RECT area{};
    std::wstring name;
    bool primary=false;
};
inline std::vector<Monitor> monitors() {
    std::vector<Monitor> result;
    EnumDisplayMonitors(nullptr,nullptr,[](HMONITOR h,HDC,LPRECT,LPARAM data)->BOOL {
        MONITORINFOEXW info{};info.cbSize=sizeof(info);
        if(GetMonitorInfoW(h,&info))
            reinterpret_cast<std::vector<Monitor>*>(data)->push_back({h,info.rcWork,info.szDevice,bool(info.dwFlags&MONITORINFOF_PRIMARY)});
        return TRUE;
    },reinterpret_cast<LPARAM>(&result));
    return result;
}
inline std::array<int,4> fontRegistrations{};
inline void registerFonts() {
    static bool loaded=false;
    if(loaded)return;
    loaded=true;
    wchar_t executable[32768]{};
    const DWORD length=GetModuleFileNameW(nullptr,executable,DWORD(std::size(executable)));
    if(!length||length>=std::size(executable))return;
    const auto base=std::filesystem::path(executable).parent_path()/L"assets"/L"widget";
    size_t index=0;
    for(const auto* file:{L"steelfish rg.ttf",L"steelfish rg it.ttf",L"la Compagnie des Ombres.ttf",L"Gravity-Light.ttf"})
        fontRegistrations[index++]=AddFontResourceExW((base/file).c_str(),FR_PRIVATE,nullptr);
}
inline HWND desktopHost() {
    HWND result=nullptr;
    EnumWindows([](HWND h,LPARAM state)->BOOL{
        if(IsWindowVisible(h)&&FindWindowExW(h,nullptr,L"SHELLDLL_DefView",nullptr)){
            *reinterpret_cast<HWND*>(state)=h;
            return FALSE;
        }
        return TRUE;
    },reinterpret_cast<LPARAM>(&result));
    return result;
}

class Clock {
public:
    Clock()=default;
    Clock(const Clock&)=delete;
    Clock& operator=(const Clock&)=delete;
    ~Clock(){stop();}
    std::function<void(const Settings&)> moved;
    std::function<void(const std::wstring&)> diagnostic;
#ifdef TLAUNCHER_NATIVE_CI
    SYSTEMTIME frozen{};
    std::vector<unsigned> ciPixels;
    SIZE ciSize{};
    std::vector<std::wstring> ciFaces;
    int ciFallbacks=0;
#endif

    void configure(const Settings& next,bool second,bool refresh=false) {
        if(!refresh&&next==config&&hwnd&&language==testi::attuale())return;
        config=next;
        secondClock=second;
        language=testi::attuale();
        if(!config.enabled){stop();return;}
        auto list=monitors();
        auto selected=std::find_if(list.begin(),list.end(),[&](const Monitor& m){return !config.display.empty()?m.name==config.display:m.primary;});
        if(selected==list.end()){stop();return;}
        if(second&&config.display.empty()&&list.size()>1){
            auto alternate=std::find_if(list.begin(),list.end(),[](const Monitor& m){return !m.primary;});
            if(alternate!=list.end())selected=alternate;
        }
        screen=*selected;
        registerFonts();
        if(!hwnd){
            WNDCLASSW cls{};cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"TLauncher.Native.DesktopClock";
            cls.lpfnWndProc=procedure;cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&cls);
            hwnd=CreateWindowExW(WS_EX_LAYERED|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,cls.lpszClassName,L"TLauncher Desktop Clock",
                WS_POPUP,0,0,1,1,nullptr,nullptr,cls.hInstance,this);
            if(!hwnd)return;
        }
        LONG_PTR flags=GetWindowLongPtrW(hwnd,GWL_EXSTYLE);
        if(config.clickThrough)flags|=WS_EX_TRANSPARENT;
        else flags&=~WS_EX_TRANSPARENT;
        SetWindowLongPtrW(hwnd,GWL_EXSTYLE,flags);
        render();
        ShowWindow(hwnd,SW_SHOWNOACTIVATE);
        order();
    }

#ifdef TLAUNCHER_NATIVE_CI
    HWND ciHandle()const{return hwnd;}
#endif
    void stop() {
        if(!hwnd)return;
        KillTimer(hwnd,1);
        if(GetCapture()==hwnd)ReleaseCapture();
        DestroyWindow(hwnd);hwnd=nullptr;
    }

    void order() {
        if(!hwnd)return;
        HWND desktop=desktopHost(),above=desktop?GetWindow(desktop,GW_HWNDPREV):nullptr;
        if(above==hwnd)return;
        const UINT flags=SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOOWNERZORDER;
        if(desktop&&above)SetWindowPos(hwnd,above,0,0,0,0,flags);
        else SetWindowPos(hwnd,HWND_BOTTOM,0,0,0,0,flags);
    }

private:
    Settings config;
    Monitor screen;
    HWND hwnd=nullptr;
    int language=-1;
    bool dragging=false,secondClock=false;
    POINT anchor{},origin{};
    std::unordered_set<std::wstring> loggedFaces;

    static LRESULT CALLBACK procedure(HWND h,UINT msg,WPARAM w,LPARAM l) {
        if(msg==WM_NCCREATE)SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams));
        auto* self=reinterpret_cast<Clock*>(GetWindowLongPtrW(h,GWLP_USERDATA));
        if(self)switch(msg){
            case WM_MOUSEACTIVATE:return MA_NOACTIVATE;
            case WM_NCHITTEST:if(self->config.clickThrough)return HTTRANSPARENT;break;
            case WM_TIMECHANGE:
            case WM_TIMER:self->render();return 0;
            case WM_LBUTTONDOWN:
                if(!self->config.locked&&!self->config.clickThrough) {
                    RECT bounds{};GetWindowRect(h,&bounds);
                    self->origin={bounds.left,bounds.top};
                    GetCursorPos(&self->anchor);
                    self->dragging=true;SetCapture(h);
                }
                return 0;
            case WM_MOUSEMOVE:
                if(self->dragging){
                    POINT point{};GetCursorPos(&point);
                    SetWindowPos(h,nullptr,self->origin.x+point.x-self->anchor.x,self->origin.y+point.y-self->anchor.y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
                }
                return 0;
            case WM_LBUTTONUP:
                if(self->dragging){self->dragging=false;ReleaseCapture();self->savePosition();}
                return 0;
            case WM_CAPTURECHANGED:self->dragging=false;return 0;
        }
        return DefWindowProcW(h,msg,w,l);
    }

    void savePosition() {
        RECT bounds{};GetWindowRect(hwnd,&bounds);
        const auto current=MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST);
        for(const auto& monitor:monitors())if(monitor.handle==current){screen=monitor;break;}
        config.display=screen.name;
        config.x=bounds.left-screen.area.left;
        config.y=bounds.top-screen.area.top;
        if(moved)moved(config);
        render();
    }

    void render() {
        if(!hwnd)return;
        SYSTEMTIME time{};GetLocalTime(&time);
#ifdef TLAUNCHER_NATIVE_CI
        if(frozen.wYear)time=frozen;
        ciFaces.clear();ciFallbacks=0;
#endif
        const int interval=config.seconds?1000-time.wMilliseconds:60000-time.wSecond*1000-time.wMilliseconds;
        SetTimer(hwnd,1,UINT(std::max(20,interval)),nullptr);
        if(dragging)return;

        UINT dpi=96,yDpi=96;
        if(FAILED(GetDpiForMonitor(screen.handle,MDT_EFFECTIVE_DPI,&dpi,&yDpi)))dpi=96;
        #ifdef TLAUNCHER_NATIVE_CI
        if(frozen.wYear)dpi=96;
#endif
        const float scale=config.scale*dpi/9600.f;
        HDC screenDC=GetDC(nullptr);
        if(!screenDC)return;
        HDC dc=CreateCompatibleDC(screenDC);
        if(!dc){ReleaseDC(nullptr,screenDC);return;}
        wchar_t hour[40]{},weekday[128]{},date[128]{};
        GetTimeFormatEx(testi::locale(),0,&time,config.seconds?L"HH:mm:ss":L"HH:mm",hour,40);
        GetDateFormatEx(testi::locale(),0,&time,L"dddd",weekday,128,nullptr);
        if(config.style==0&&testi::attuale()==testi::INGLESE){
            wchar_t month[48]{};GetDateFormatEx(L"en-US",0,&time,L"MMMM",month,48,nullptr);
            const auto day=time.wDay;
            const wchar_t* suffix=day>=11&&day<=13?L"th":day%10==1?L"st":day%10==2?L"nd":day%10==3?L"rd":L"th";
            swprintf(date,128,L"%u%ls %ls %u",unsigned(day),suffix,month,unsigned(time.wYear));
        }else GetDateFormatEx(testi::locale(),0,&time,L"d MMMM yyyy",date,128,nullptr);
        if(config.style==0)CharUpperBuffW(date,DWORD(wcslen(date)));
        const bool classic=config.style==0;
        const wchar_t* straight=config.font.empty()?(classic?L"Steelfish Rg":L"Gravity"):config.font.c_str();
        const wchar_t* script=config.font.empty()&&classic?L"la Compagnie des Ombres":straight;
        struct Accent {size_t index;wchar_t mark;};
        struct Run {std::wstring text;HFONT font;COLORREF color;RECT rect;SCRIPT_STRING_ANALYSIS shaping=nullptr;std::vector<Accent> accents;};
        std::vector<Run> runs;
        RECT canvas{0,0,LONG(320*scale),LONG((config.date?(classic?230:135):105)*scale)};
        const auto layout=[&](const wchar_t* value,const wchar_t* face,int size,int base,COLORREF color){
            const int px=std::max(1,int(size*scale));
            HFONT font=CreateFontW(classic?px:-px,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,face);
            if(!font)return;
            const auto previous=SelectObject(dc,font);SIZE extent{};
            wchar_t selected[LF_FACESIZE]{};GetTextFaceW(dc,LF_FACESIZE,selected);
            std::vector<WORD> glyphs(wcslen(value));
            const DWORD glyphResult=GetGlyphIndicesW(dc,value,int(glyphs.size()),glyphs.data(),GGI_MARK_NONEXISTING_GLYPHS);
            SCRIPT_STRING_ANALYSIS shaping=nullptr;
            std::wstring rendered=value;std::vector<Accent> accents;
            const bool missing=glyphResult==GDI_ERROR||std::find(glyphs.begin(),glyphs.end(),WORD(0xffff))!=glyphs.end();
            if(missing&&testi::attuale()==testi::VIETNAMITA){
                const int n=NormalizeString(NormalizationD,value,int(wcslen(value)),nullptr,0);
                if(n>0){
                    std::wstring decomposed(size_t(n),L'\0');
                    const int count=NormalizeString(NormalizationD,value,int(wcslen(value)),decomposed.data(),n);
                    if(count>0){rendered.clear();for(int i=0;i<count;++i){
                        const wchar_t c=decomposed[i];
                        if(c>=0x300&&c<=0x36f){if(!rendered.empty())accents.push_back({rendered.size()-1,c});}
                        else if(c==L'Đ'||c==L'đ'){rendered+=c==L'Đ'?L'D':L'd';accents.push_back({rendered.size()-1,0x335});}
                        else rendered+=c;
                    }}
                }
            }else if(missing)ScriptStringAnalyse(dc,value,int(wcslen(value)),0,-1,SSA_GLYPHS|SSA_FALLBACK|SSA_LINK,0,nullptr,nullptr,nullptr,nullptr,nullptr,&shaping);
            if(shaping)extent=*ScriptString_pSize(shaping);
            else GetTextExtentPoint32W(dc,rendered.c_str(),int(rendered.size()),&extent);
            const auto message=std::wstring(L"clock requested=")+face+L" selected="+selected+(accents.empty()?(missing?L" glyph-link=required":L" glyph-link=none"):L" glyph-link=decomposed-native-diacritics");
            if(diagnostic&&loggedFaces.insert(message).second)diagnostic(message);
#ifdef TLAUNCHER_NATIVE_CI
            ciFaces.push_back(selected);if(shaping||!accents.empty())++ciFallbacks;
#endif
            SelectObject(dc,previous);
            const LONG halfWidth=(std::max(1L,extent.cx)+1)/2;
            const LONG center=classic?120:canvas.right/2,top=LONG(base*scale);
            RECT rect{center-halfWidth,top,center+halfWidth,top+px+8};
            const LONG padding=classic?std::max(8,px):8;
            canvas.left=std::min(canvas.left,rect.left-padding);canvas.top=std::min(canvas.top,rect.top-padding);
            canvas.right=std::max(canvas.right,rect.right+padding);canvas.bottom=std::max(canvas.bottom,rect.bottom+padding);
            runs.push_back({std::move(rendered),font,color,rect,shaping,std::move(accents)});
        };
        layout(hour,straight,classic?100:70,0,config.color);
        if(config.date){
            layout(date,straight,classic?35:27,classic?150:86,config.color);
            if(classic)layout(weekday,script,85,70,config.color==RGB(185,231,245)?RGB(220,247,255):config.color);
        }
        const int width=std::max(1,std::min(int(screen.area.right-screen.area.left),int(canvas.right-canvas.left)));
        const int height=std::max(1,std::min(int(screen.area.bottom-screen.area.top),int(canvas.bottom-canvas.top)));
        BITMAPINFO info{};info.bmiHeader={sizeof(BITMAPINFOHEADER),width,-height,1,32,BI_RGB,0,0,0,0};
        void* data=nullptr;
        HBITMAP bitmap=CreateDIBSection(screenDC,&info,DIB_RGB_COLORS,&data,nullptr,0);
        if(!bitmap||!data){
            if(bitmap)DeleteObject(bitmap);
            for(auto& run:runs){if(run.shaping)ScriptStringFree(&run.shaping);DeleteObject(run.font);}
            DeleteDC(dc);ReleaseDC(nullptr,screenDC);
            return;
        }
        auto old=SelectObject(dc,bitmap);
        auto* pixels=static_cast<unsigned*>(data);
        std::fill_n(pixels,size_t(width)*height,0u);
        SetBkMode(dc,TRANSPARENT);
        for(auto& run:runs){
            RECT rect=run.rect;OffsetRect(&rect,-canvas.left,-canvas.top);
            const auto previous=SelectObject(dc,run.font);SetTextColor(dc,run.color);
            if(run.shaping){
                const SIZE extent=*ScriptString_pSize(run.shaping);
                ScriptStringOut(run.shaping,(rect.left+rect.right-extent.cx)/2,(rect.top+rect.bottom-extent.cy)/2,0,nullptr,0,0,FALSE);
                ScriptStringFree(&run.shaping);
            }else{
                DrawTextW(dc,run.text.c_str(),-1,&rect,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX|DT_NOCLIP);
                if(!run.accents.empty()){
                    TEXTMETRICW metrics{};GetTextMetricsW(dc,&metrics);SIZE full{};GetTextExtentPoint32W(dc,run.text.c_str(),int(run.text.size()),&full);
                    const int origin=(rect.left+rect.right-full.cx)/2;
                    const int baseline=(rect.top+rect.bottom-metrics.tmHeight)/2+metrics.tmAscent;
                    const MAT2 identity{{0,1},{0,0},{0,0},{0,1}};
                    HPEN accentPen=CreatePen(PS_SOLID,std::max(1,int(metrics.tmHeight*.025)),run.color);
                    const auto oldPen=SelectObject(dc,accentPen);
                    auto line=[&](int x1,int y1,int x2,int y2){MoveToEx(dc,x1,y1,nullptr);LineTo(dc,x2,y2);};
                    for(const auto& accent:run.accents){
                        SIZE prefix{};GetTextExtentPoint32W(dc,run.text.c_str(),int(accent.index),&prefix);
                        GLYPHMETRICS glyph{};if(GetGlyphOutlineW(dc,run.text[accent.index],GGO_METRICS,&glyph,0,nullptr,&identity)==GDI_ERROR)continue;
                        const int x=origin+prefix.cx+glyph.gmptGlyphOrigin.x+int(glyph.gmBlackBoxX)/2;
                        const int inkTop=baseline-glyph.gmptGlyphOrigin.y;
                        const int unit=std::max(2,int(metrics.tmHeight*.055));
                        const int y=inkTop-unit,half=std::max(unit,int(glyph.gmBlackBoxX)/3);
                        switch(accent.mark){
                            case 0x301:line(x-unit,y-unit,x+unit,y-2*unit);break;
                            case 0x300:line(x-unit,y-2*unit,x+unit,y-unit);break;
                            case 0x302:line(x-half,y,x,y-unit);line(x,y-unit,x+half,y);break;
                            case 0x306:line(x-half,y-unit,x-half/2,y);line(x-half/2,y,x+half/2,y);line(x+half/2,y,x+half,y-unit);break;
                            case 0x303:line(x-half,y,x-half/2,y-unit);line(x-half/2,y-unit,x+half/2,y);line(x+half/2,y,x+half,y-unit);break;
                            case 0x309:line(x,y-2*unit,x+unit,y-2*unit);line(x+unit,y-2*unit,x+unit,y-unit);line(x+unit,y-unit,x,y);break;
                            case 0x31b:line(x+half,inkTop+unit,x+half+unit,inkTop);line(x+half+unit,inkTop,x+half+unit,inkTop-unit);break;
                            case 0x323:{const int bottom=baseline-glyph.gmptGlyphOrigin.y+int(glyph.gmBlackBoxY)+unit;line(x,bottom,x+1,bottom);break;}
                            case 0x335:line(x-half,baseline-int(glyph.gmBlackBoxY)/2,x+half,baseline-int(glyph.gmBlackBoxY)/2);break;
                        }
                    }
                    SelectObject(dc,oldPen);DeleteObject(accentPen);
                }
            }
            SelectObject(dc,previous);DeleteObject(run.font);
        }
        GdiFlush();
        for(size_t i=0;i<size_t(width)*height;++i){
            const unsigned rgb=pixels[i]&0xffffff;
            const unsigned alpha=std::max({rgb&255,(rgb>>8)&255,rgb>>16});
            pixels[i]=rgb|(alpha<<24);
        }
#ifdef TLAUNCHER_NATIVE_CI
        ciPixels.assign(pixels,pixels+size_t(width)*height);ciSize={width,height};
#endif
        const auto& area=screen.area;
        POINT pos{area.left+(config.x==-1?(area.right-area.left-width)/2:config.x),
                  area.top+(config.y==-1?int((secondClock&&screen.primary?300:70)*scale):config.y)};
        pos.x=std::clamp(pos.x,area.left,area.right-width);
        pos.y=std::clamp(pos.y,area.top,area.bottom-height);
        SIZE size{width,height};POINT source{};
        BLENDFUNCTION blend{AC_SRC_OVER,0,BYTE(config.opacity*255/100),AC_SRC_ALPHA};
        UpdateLayeredWindow(hwnd,screenDC,&pos,&size,dc,&source,0,&blend,ULW_ALPHA);
        SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(nullptr,screenDC);
    }
};
} // namespace desktopclock
