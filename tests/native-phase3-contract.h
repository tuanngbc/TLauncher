#include "clock-v102-reference.h"
// Compiled/executed exclusively on the hosted Windows runner.
#pragma once
static bool phase3Snapshot(HWND h,const std::filesystem::path& file) {
    RECT r{};GetWindowRect(h,&r);int w=r.right-r.left,hh=r.bottom-r.top;if(w<=0||hh<=0)return false;
    HDC screen=GetDC(nullptr),dc=CreateCompatibleDC(screen);void* pixels=nullptr;
    BITMAPINFO info{};info.bmiHeader={sizeof(BITMAPINFOHEADER),w,-hh,1,32,BI_RGB,0,0,0,0,0};
    HBITMAP bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&pixels,nullptr,0);bool ok=false;
    if(bitmap&&pixels&&dc){auto old=SelectObject(dc,bitmap);ok=BitBlt(dc,0,0,w,hh,screen,r.left,r.top,SRCCOPY|CAPTUREBLT);GdiFlush();
        BITMAPFILEHEADER head{};head.bfType=0x4d42;head.bfOffBits=sizeof(head)+sizeof(info.bmiHeader);head.bfSize=head.bfOffBits+w*hh*4;
        std::ofstream out(file,std::ios::binary);out.write((char*)&head,sizeof(head));out.write((char*)&info.bmiHeader,sizeof(info.bmiHeader));out.write((char*)pixels,size_t(w)*hh*4);ok=ok&&bool(out);SelectObject(dc,old);}
    if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);ReleaseDC(nullptr,screen);return ok;
}
static int phase3ShortcutContracts(std::ostream& report){
    shortcuts::Key key{};
    if(!shortcuts::parse(L" Ctrl + Alt + F23 ",key)||key!=shortcuts::Key{MOD_CONTROL|MOD_ALT,VK_F23})return 107;
    if(shortcuts::parse(L"Ctrl+F12",key)||shortcuts::parse(L"Alt+A+B",key)||shortcuts::parse(L"S",key)||shortcuts::parse(L"Ctrl+F2+",key))return 108;
    struct Blocker{
        HANDLE ready=CreateEventW(nullptr,TRUE,FALSE,nullptr),stop=CreateEventW(nullptr,TRUE,FALSE,nullptr);bool ok=false;std::thread thread;
        Blocker(){thread=std::thread([this]{ok=RegisterHotKey(nullptr,0x5402,MOD_CONTROL|MOD_ALT|MOD_SHIFT|MOD_NOREPEAT,VK_F24)!=FALSE;
            SetEvent(ready);WaitForSingleObject(stop,10000);if(ok)UnregisterHotKey(nullptr,0x5402);});}
        void finish(){SetEvent(stop);if(thread.joinable())thread.join();}
        ~Blocker(){finish();CloseHandle(ready);CloseHandle(stop);}
    } blocker;
    if(WaitForSingleObject(blocker.ready,3000)!=WAIT_OBJECT_0||!blocker.ok)return 109;
    HWND destination=CreateWindowW(L"STATIC",L"Shortcut contract",0,0,0,10,10,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!destination)return 110;
    struct Cleanup{HWND h;~Cleanup(){DestroyWindow(h);}} cleanup{destination};
    shortcuts::Binding binding{0x5400,0x5401};const UINT mods=MOD_CONTROL|MOD_ALT|MOD_SHIFT;
    if(!binding.apply(destination,{mods,VK_F23},true))return 111;
    if(binding.apply(destination,{mods,VK_F24},true)||!binding.active()||binding.key().key!=VK_F23)return 112;
    bool oldAvailable=true;std::thread probe([&]{oldAvailable=RegisterHotKey(nullptr,0x5403,mods|MOD_NOREPEAT,VK_F23)!=FALSE;if(oldAvailable)UnregisterHotKey(nullptr,0x5403);});probe.join();
    if(oldAvailable)return 113;
    blocker.finish();if(!binding.apply(destination,{mods,VK_F24},true)||binding.key().key!=VK_F24)return 114;
    std::thread freeProbe([&]{oldAvailable=RegisterHotKey(nullptr,0x5403,mods|MOD_NOREPEAT,VK_F23)!=FALSE;if(oldAvailable)UnregisterHotKey(nullptr,0x5403);});freeProbe.join();
    if(!oldAvailable)return 115;
    binding.stop();if(binding.active())return 116;
    report<<"SHORTCUT_CUSTOM_PARSER=PASS\nSHORTCUT_ACTUAL_WINDOWS_CONFLICT_PRESERVES_OLD=PASS\nSHORTCUT_GAP_FREE_REPLACEMENT_AND_RELEASE=PASS\n";return 0;
}
static int phase3DockContracts(std::ostream& report,const std::filesystem::path& evidence){
    HWND source=CreateWindowW(L"STATIC",L"Dock hosted interaction fixture",WS_OVERLAPPEDWINDOW|WS_VISIBLE,310,180,300,180,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!source)return 150;
    POINT original{};GetCursorPos(&original);MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(source,MONITOR_DEFAULTTONEAREST),&monitor);
    struct Cleanup{HWND source;POINT cursor;~Cleanup(){gDock.chiudi();gDock.fermaWidget(3000);SetCursorPos(cursor.x,cursor.y);DestroyWindow(source);}}cleanup{source,original};
    gDock.impostaSpazio(false);gDock.impostaNotchEnabled(false);gDock.altriSchermi=false;
    const RECT originalWork=monitor.rcWork;
    if(!gDock.inizializza(GetModuleHandleW(nullptr)))return 151;
    // Accepted Dock startup enables Windows taskbar auto-hide. Compare collapse
    // transitions against that active-shell baseline, then verify full recovery.
    ciPump(350);GetMonitorInfoW(MonitorFromWindow(source,MONITOR_DEFAULTTONEAREST),&monitor);
    SetForegroundWindow(source);const POINT away{(monitor.rcMonitor.left+monitor.rcMonitor.right)/2,(monitor.rcMonitor.top+monitor.rcMonitor.bottom)/2};SetCursorPos(away.x,away.y);
    auto pump=[&](int milliseconds){ULONGLONG until=GetTickCount64()+milliseconds;do{ciPump(8);gDock.ciBorder();if(gDock.serveFotogramma())gDock.disegna(vetro::secondi(),false);}while(GetTickCount64()<until);};
    auto await=[&](auto condition){const ULONGLONG until=GetTickCount64()+6000;do{pump(20);if(condition())return true;}while(GetTickCount64()<until);return false;};
    for(int edge=0;edge<4;++edge){
        ShowWindow(source, SW_RESTORE);
        SetWindowPos(source,nullptr,monitor.rcMonitor.left+140,monitor.rcMonitor.top+140,200,130,SWP_NOZORDER);
        SetForegroundWindow(source);
        gDock.impostaLato(edge);
        gDock.impostaVisibility(2,64,4,80,120,180,false,8,3,35,2,12);
        gDock.impostaSpazio(true);
        gDock.controllaFinestre(true);
        if(!await([&]{return gDock.ciCollapse()<.001&&!gDock.serveFotogramma();}))return 181;
        ShowWindow(source, SW_MAXIMIZE);
        SetForegroundWindow(source);
        gDock.controllaFinestre(true);
        if(!await([&]{return gDock.ciCollapse()>.999&&!gDock.serveFotogramma();}))return 152;
        RECT bar=gDock.ciCollapsedBounds(),overlap{};if(!IntersectRect(&overlap,&bar,&monitor.rcMonitor)||!EqualRect(&bar,&overlap))return 153;
        if(edge<2?bar.right-bar.left<=bar.bottom-bar.top:bar.bottom-bar.top<=bar.right-bar.left)return 154;
        const POINT hover{(bar.left+bar.right)/2,(bar.top+bar.bottom)/2};SetCursorPos(hover.x,hover.y);
        if(!await([&]{return gDock.ciCollapse()<.001;}))return 155;
        RECT expandedAnchor{};if(!gDock.ciAnchor(expandedAnchor)||gDock.latoPer(expandedAnchor)!=edge)return 156;
        SetCursorPos(away.x,away.y);
        if(!await([&]{return gDock.ciCollapse()>.999&&!gDock.serveFotogramma();}))return 157;
        RECT collapsedAnchor{};if(!gDock.ciAnchor(collapsedAnchor)||!EqualRect(&expandedAnchor,&collapsedAnchor))return 158;
        if(!await([&]{return gDock.ciCollapse()>.999&&!gDock.serveFotogramma();}))return 159;
        const long frames=gDock.diagFotogrammi;pump(400);if(gDock.diagFotogrammi!=frames)return 160;
        if(!phase3Snapshot(gDock.icone,evidence/(L"phase3-dock-collapsed-edge-"+std::to_wstring(edge)+L".bmp")))return 161;
        MONITORINFO hoverWork{sizeof(hoverWork)};
        GetMonitorInfoW(MonitorFromWindow(source,MONITOR_DEFAULTTONEAREST),&hoverWork);
        if(!EqualRect(&monitor.rcWork,&hoverWork.rcWork))return 182;
        ShowWindow(source, SW_RESTORE);
        gDock.controllaFinestre(true);
        if(!await([&]{return gDock.ciCollapse()<.001&&!gDock.serveFotogramma();}))return 183;
        report<<"DOCK_NATIVE_EDGE="<<edge<<" HOVER_RETRACT_ANCHOR_AND_IDLE=PASS\n";report.flush();
    }
    gDock.schermoIntero(2);if(IsWindowVisible(gDock.icone)||gDock.serveFotogramma())return 162;
    gDock.schermoIntero(0);pump(600);if(!IsWindowVisible(gDock.icone))return 163;
    MONITORINFO unchanged{sizeof(unchanged)};GetMonitorInfoW(MonitorFromWindow(source,MONITOR_DEFAULTTONEAREST),&unchanged);
    if(!EqualRect(&monitor.rcWork,&unchanged.rcWork))return 164;
    gDock.schermoIntero(2);
    if(!gDock.ciSecondaryPresentation(MonitorFromWindow(source,MONITOR_DEFAULTTONEAREST),[&](HWND h,const std::wstring& name){return phase3Snapshot(h,evidence/(L"phase3-"+name+L".bmp"));}))return 198;
    report<<"SECONDARY_PRESENTATION_REAL_HWND_HOVER_MAGNIFICATION_SETTLES=PASS\nSECONDARY_RADIUS_LIGHT_DARK_EXPANDED_COLLAPSED_SCREENSHOTS=EXPORTED\nPHYSICAL_DUAL_MONITOR_OWNER_UAT=PENDING\n";
    gDock.chiudi();const ULONGLONG restoredUntil=GetTickCount64()+3000;
    do{ciPump(50);GetMonitorInfoW(MonitorFromWindow(source,MONITOR_DEFAULTTONEAREST),&unchanged);}while(!EqualRect(&originalWork,&unchanged.rcWork)&&GetTickCount64()<restoredUntil);
    if(!EqualRect(&originalWork,&unchanged.rcWork))return 165;
    report<<"DOCK_ORIGINAL_WORK_AREA_RESTORED_ON_EXIT=PASS\n";
    report<<"DOCK_COLLAPSED_STABLE_ANIMATION_ANCHOR=PASS\nDOCK_COLLAPSED_SETTLED_NO_RENDER_LOOP=PASS\nDOCK_FULLSCREEN_HIDE_RESUME=PASS\nDOCK_WORK_AREA_UNCHANGED=PASS\n";
    report<<"DOCK_COLLAPSED_FOUR_EDGE_NATIVE_INTERACTION=PASS\n";return 0;
}
static int phase3Contracts(std::ostream& report,const std::filesystem::path& evidence) {
    if(int result=phase3ShortcutContracts(report))return result;
    {
        resource_readout::Readout readout;
        const auto deadline=GetTickCount64()+8000;
        do {
            readout.update();
            if(readout.values().cpu>=0&&readout.values().ram>=0)break;
            ciPump(20);
        } while(GetTickCount64()<deadline);
        if(readout.values().cpu<0||readout.values().cpu>100||
            readout.values().ram<0||readout.values().ram>100)return 192;
        readout.stop();
        if(readout.values().cpu!=-1||readout.values().ram!=-1)return 193;
        report<<"CONTROL_CENTER_RESOURCE_SAMPLES_AND_RESET=PASS\n";
    }
    {
        appicons::Pixels fixture{48,48,std::vector<UINT>(48*48)};
        for(int y=18;y<30;++y)for(int x=12;x<36;++x)fixture.bgra[y*48+x]=0xff3695e4;
        appicons::normalize(fixture,48);
        if(fixture.w!=48||fixture.h!=48)return 189;
        int left=48,right=0,top=48,bottom=0;
        for(int y=0;y<48;++y)for(int x=0;x<48;++x)if(fixture.bgra[y*48+x]>>24){left=std::min(left,x);right=std::max(right,x+1);top=std::min(top,y);bottom=std::max(bottom,y+1);}
        if(right-left!=43||bottom-top!=22||fixture.bgra[24*48+24]!=0xff3695e4)return 189;
        appicons::Pixels tiny{48,48,std::vector<UINT>(48*48)};for(int y=21;y<27;++y)for(int x=21;x<27;++x)tiny.bgra[y*48+x]=0xff3695e4;
        appicons::normalize(tiny,48);
        // Enlargement is bounded at 4x to avoid destroying very small artwork.
        if(tiny.bgra[24*48+11]||tiny.bgra[24*48+12]!=0xff3695e4||tiny.bgra[24*48+35]!=0xff3695e4||tiny.bgra[24*48+36])return 189;
        if(std::wstring(appicons::normalizationVersion)!=L"optical-v2|")return 189;
        clockreference::save({{48,48},fixture.bgra},evidence/L"icon-optical-wide.bmp");
        report<<"ICON_OPTICAL_V2_TRANSPARENT_PADDING_ASPECT_AND_VERSION=PASS\n";
    }
    {
        desktopclock::Clock clock;desktopclock::Settings appearance;appearance.enabled=true;
        clock.frozen={2026,9,1,28,23,58,0,0};
        const int locale=testi::lingua().load();
        for(const auto lang:{testi::INGLESE,testi::VIETNAMITA})for(int scale:{70,100,140}){
            testi::lingua()=lang;appearance.scale=scale;clock.configure(appearance,false,true);
            const auto window=clock.ciHandle();
            if(!window){testi::lingua()=locale;return 190;}
            if(clock.ciFaces.size()!=3||_wcsicmp(clock.ciFaces[0].c_str(),L"Steelfish Rg")||_wcsicmp(clock.ciFaces[2].c_str(),L"la Compagnie des Ombres"))return 195;
            for(int n:desktopclock::fontRegistrations)if(n<=0)return 195;
            const auto tag=std::wstring(lang==testi::INGLESE?L"en-":L"vi-")+std::to_wstring(scale);
            const auto reference=clockreference::render(scale/100.f,clock.frozen,lang==testi::VIETNAMITA);
            clockreference::save(reference,evidence/(L"clock-v102-reference-"+tag+L".bmp"));
            clockreference::save({clock.ciSize,clock.ciPixels},evidence/(L"clock-candidate-"+tag+L".bmp"));
            if(lang==testi::INGLESE&&(reference.size.cx!=clock.ciSize.cx||reference.size.cy!=clock.ciSize.cy||reference.pixels!=clock.ciPixels))return 196;
            report<<"CLOCK_ACTUAL_FACES=Steelfish Rg,la Compagnie des Ombres SCALE="<<scale<<" LANGUAGE="<<lang<<" GLYPH_LINKED_RUNS="<<clock.ciFallbacks<<"\n";
        }
        clock.stop();testi::lingua()=locale;
        report<<"CLOCK_V102_FROZEN_EN_PIXEL_PARITY=PASS\nCLOCK_VI_GLYPH_LINK_VISUAL_COMPARISON=EXPORTED_OWNER_UAT_PENDING\n";
    }
    {
        const int language=testi::lingua().exchange(testi::CINESE);
        const bool valid=!wcscmp(testi::nome(testi::CINESE),L"简体中文")&&!wcscmp(testi::locale(),L"zh-CN")&&
            !wcscmp(testi::t(L"Lingua",L"Language"),L"语言")&&
            !wcscmp(testi::t(L"Dock",L"Dock"),L"程序坞")&&
            !wcscmp(testi::t(nullptr,L"unmapped synthetic fixture"),L"unmapped synthetic fixture");
        testi::lingua()=language;
        if(!valid)return 194;
        report<<"NATIVE_SIMPLIFIED_CHINESE_SETTINGS_LOOKUP_AND_FALLBACK=PASS\n";
    }
    if(appcatalog::fold(L"Tiếng Việt Đặng")!=L"tieng viet dang")return 60;
    std::filesystem::create_directories(tlauncher::dockPins());
    auto pin=tlauncher::dockPins()/L"fixture.lnk";
    if(!tlauncher::ownsDockPin(pin.wstring())||tlauncher::ownsDockPin(L"C:\\Windows\\fixture.lnk"))return 82;
    report<<"DOCK_PIN_WRITE_BOUNDARY=PASS\n";
    auto& s=impostazioni::valori();s.stage.height=174;s.stage.peekDelay=750;s.stage.peekLive=false;s.dockVisibility=2;s.dockBarCloseDelay=900;s.spotlightMode=2;s.spotlightShortcut={MOD_CONTROL|MOD_ALT,VK_F22};s.stageShortcut={MOD_ALT|MOD_SHIFT,'S'};s.stageShortcutEnabled=false;impostazioni::salva();
    const auto capture=s.captureKeys;
    s.captureKeys={{{MOD_CONTROL|MOD_ALT,VK_F19},{MOD_CONTROL|MOD_ALT,VK_F20},{MOD_CONTROL|MOD_ALT,VK_F21}}};impostazioni::salva();
    s.captureKeys=capture;
    s.stage.height=72;s.dockVisibility=0;impostazioni::carica();
    if(s.captureKeys!=std::array<shortcuts::Key,3>{{{MOD_CONTROL|MOD_ALT,VK_F19},{MOD_CONTROL|MOD_ALT,VK_F20},{MOD_CONTROL|MOD_ALT,VK_F21}}})return 197;
    s.captureKeys=capture;impostazioni::salva();
    report<<"THREE_CAPTURE_SHORTCUTS_PERSISTENCE=PASS\n";
    if(s.stage.height!=174||s.stage.peekDelay!=750||s.stage.peekLive||s.dockVisibility!=2||s.dockBarCloseDelay!=900||s.spotlightMode!=2||s.spotlightShortcut!=shortcuts::Key{MOD_CONTROL|MOD_ALT,VK_F22}||s.stageShortcut!=shortcuts::Key{MOD_ALT|MOD_SHIFT,'S'}||s.stageShortcutEnabled)return 61;
    appcatalog::Metadata meta;meta.name=L"Ứng dụng thử nghiệm";meta.favorite=true;meta.folder=L"Công việc";meta.manual=true;meta.icon=L"custom.ico";
    appcatalog::save(L"C:\\fixture\\tool.exe",meta);appcatalog::folder(L"",meta.folder);appcatalog::loaded=false;appcatalog::metadata.clear();
    auto restored=appcatalog::get(L"C:\\fixture\\tool.exe");if(restored.name!=meta.name||!restored.favorite||restored.folder!=meta.folder||!restored.manual||restored.icon!=meta.icon)return 62;
    appcatalog::folder(meta.folder,L"Đã đổi tên");if(appcatalog::get(L"C:\\fixture\\tool.exe").folder!=L"Đã đổi tên")return 63;
    appcatalog::folder(L"Đã đổi tên",L"");if(appcatalog::get(L"C:\\fixture\\tool.exe").manual!=true||!appcatalog::get(L"C:\\fixture\\tool.exe").folder.empty())return 64;
    const std::wstring alias=L"C:\\fixture\\tool-alias.lnk",canonical=L"exe:c:\\fixture\\tool.exe";
    appcatalog::bind(L"C:\\fixture\\tool.exe",canonical);appcatalog::bind(alias,canonical);
    auto shared=appcatalog::get(alias);if(shared.name!=meta.name||shared.icon!=meta.icon||!shared.favorite)return 98;
    shared.name=L"Tên mới dùng chung";shared.folder=L"Dự án";shared.icon=L"new-custom.png";shared.hidden=true;appcatalog::save(alias,shared);
    auto original=appcatalog::get(L"C:\\fixture\\tool.exe");
    if(original.name!=shared.name||original.icon!=shared.icon||!original.hidden||!original.manual)return 99;
    appcatalog::folder(L"Dự án",L"Đã gộp");if(appcatalog::get(alias).folder!=L"Đã gộp"||appcatalog::get(alias).name!=shared.name)return 100;
    appcatalog::loaded=false;appcatalog::metadata.clear();appcatalog::aliases.clear();appcatalog::owners.clear();appcatalog::load();
    appcatalog::bind(L"C:\\fixture\\tool.exe",canonical);appcatalog::bind(alias,canonical);
    if(appcatalog::get(L"C:\\fixture\\tool.exe").name!=shared.name||appcatalog::iconSource(alias)!=shared.icon)return 101;
    auto shown=appcatalog::get(alias);shown.hidden=false;appcatalog::save(alias,shown);
    wchar_t actualExe[32768]{};GetModuleFileNameW(nullptr,actualExe,32768);
    IShellLinkW* link=nullptr;IPersistFile* persist=nullptr;
    if(FAILED(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&link))))return 102;
    const auto shortcut=std::filesystem::absolute(evidence/L"canonical-fixture.lnk");link->SetPath(actualExe);
    if(FAILED(link->QueryInterface(IID_PPV_ARGS(&persist)))||FAILED(persist->Save(shortcut.c_str(),TRUE)))return 103;
    const auto exeIdentity=appcatalog::identity(actualExe),linkIdentity=appcatalog::identity(shortcut.wstring());
    if(exeIdentity!=linkIdentity)return 104;
    link->SetArguments(L"--separate-fixture");if(FAILED(persist->Save(shortcut.c_str(),TRUE)))return 105;
    persist->Release();link->Release();
    if(appcatalog::identity(shortcut.wstring())==exeIdentity)return 106;
    report<<"ACTUAL_EXE_LNK_IDENTITY_AND_ARGUMENT_DISTINCTION=PASS\n";
    report<<"CANONICAL_METADATA_ALIAS_PROPAGATION=PASS\nCANONICAL_METADATA_RELOAD_AND_FOLDER_MIGRATION=PASS\n";
    report<<"PHASE3_CUSTOM_SETTINGS_PERSIST=PASS\nVIETNAMESE_ACCENT_SEARCH=PASS\nCATALOG_FOLDER_RENAME_DELETE_PRESERVE_APPS=PASS\n";
    {
        wallpaper::SettingsPage page;wallpaper::Config choiceConfig;choiceConfig.scene=wallpaper::sceneIndex("boxing-ring");
        page.read=[&]{return choiceConfig;};page.changed=[&](wallpaper::Config value){choiceConfig=std::move(value);};
        for(const auto& setting:std::vector<std::pair<const char*,int>>{{"autoSpar",2},{"gloveStyle",3},{"hudMode",4}}){
            const auto labels=wallpaper::SettingsPage::options("boxing-ring",setting.first);if(labels.size()!=3)return 182;
            for(int option=0;option<3;++option)if(!page.click(5600+setting.second*4+option)||wallpaper::parameter(choiceConfig,choiceConfig.scene,setting.first)!=option)return 183;
            if(page.click(5600+setting.second*4+3))return 184;
        }
        if(wallpaper::SettingsPage::options("boxing-ring","autoSpar")[0]!=L"Đấu nhịp tự nhiên")return 185;
        report<<"BOXING_NAMED_LOCALIZED_OPTIONS_AND_NATIVE_SETTINGS_ROUTE=PASS\n";
    }
    HWND source=CreateWindowW(L"STATIC",L"Stage actual source window",WS_OVERLAPPEDWINDOW|WS_VISIBLE,280,160,640,420,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!source)return 65;ciPump(100);DWORD pid=0;DWORD tid=GetWindowThreadProcessId(source,&pid);
    stage::Manager manager;manager.inventory=[&]{return IsWindow(source)?std::vector<stage::Window>{{source,pid,tid,L"fixture",L"Cửa sổ thử nghiệm",L""}}:std::vector<stage::Window>{};};
    bool activated=false;manager.activation=[&](HWND h){activated=h==source;};stage::Settings st;st.enabled=true;st.peekDelay=100;manager.configure(st);ciPump(180);manager.changed();ciPump(60);
    HWND rail=FindWindowW(L"TLauncher.Native.Stage",nullptr);if(!rail||manager.windowCount()!=1)return 66;
    WINDOWPLACEMENT before{sizeof(before)},after{sizeof(after)};GetWindowPlacement(source,&before);HWND focus=GetForegroundWindow();
    POINT previousPointer{};GetCursorPos(&previousPointer);POINT hoverPoint{45,45};ClientToScreen(rail,&hoverPoint);
    if(!SetCursorPos(hoverPoint.x,hoverPoint.y))return 79;ciPump(180);
    HWND peek=FindWindowW(L"TLauncher.Native.StagePeek",nullptr);if(!peek)return 67;
    GetWindowPlacement(source,&after);if(memcmp(&before,&after,sizeof(before))||focus!=GetForegroundWindow())return 68;
    if(!phase3Snapshot(rail,evidence/L"phase3-stage.bmp")||!phase3Snapshot(peek,evidence/L"phase3-peek.bmp"))return 69;
    // Crossing rail -> Peek must keep the preview clickable; leaving both dismisses it.
    POINT previewPoint{12,12};ClientToScreen(peek,&previewPoint);SetCursorPos(previewPoint.x,previewPoint.y);
    ciPump(220);peek=FindWindowW(L"TLauncher.Native.StagePeek",nullptr);if(!peek)return 70;
    SendMessageW(peek,WM_LBUTTONUP,0,MAKELPARAM(12,12));if(!activated)return 71;activated=false;
    report<<"STAGE_PEEK_CLICK_ACTIVATION=PASS\n";
    SendMessageW(rail,WM_LBUTTONUP,0,MAKELPARAM(45,45));if(!activated)return 87;
    ShowWindow(source,SW_MINIMIZE);ciPump(100);manager.changed();ciPump(80);
    if(!IsIconic(source)||!manager.snapshotBytes()||!manager.cachedPreviewCount())return 80;
    if(!phase3Snapshot(rail,evidence/L"phase3-stage-minimized.bmp"))return 81;
    report<<"STAGE_MINIMIZED_SHARED_SNAPSHOT_NO_RESTORE=PASS\n";
    st.perspective=true;manager.configure(st);ciPump(100);manager.changed();rail=FindWindowW(L"TLauncher.Native.Stage",nullptr);
    if(!rail||!IsIconic(source)||!manager.cachedPreviewCount()||!phase3Snapshot(rail,evidence/L"phase3-stage-perspective.bmp"))return 93;
    report<<"STAGE_NATIVE_PROJECTIVE_CARD_RENDER=PASS\n";
    for(bool perspective:{false,true}){
        auto transparent=st;transparent.background=0;transparent.railOpacity=0;transparent.cardOpacity=0;transparent.perspective=perspective;
        manager.configure(transparent);ciPump(120);manager.changed();rail=FindWindowW(L"TLauncher.Native.Stage",nullptr);
        if(!rail||!phase3Snapshot(rail,evidence/(perspective?L"phase3-stage-transparent-perspective.bmp":L"phase3-stage-transparent-flat.bmp")))return 199;
    }
    manager.configure(st);ciPump(100);manager.changed();rail=FindWindowW(L"TLauncher.Native.Stage",nullptr);
    report<<"STAGE_ZERO_RAIL_AND_CARD_OPACITY_NO_OUTLINES=EXPORTED\n";
    // Actual rail geometry follows each synthetic Dock edge footprint without
    // changing the real Windows work area, even when inventory is unchanged.
    MONITORINFO monitor{sizeof(monitor)};HMONITOR display=MonitorFromWindow(rail,MONITOR_DEFAULTTONEAREST);GetMonitorInfoW(display,&monitor);RECT reservation{};
    manager.reserved=[&](HMONITOR){return reservation;};
    for(int edge=0;edge<4;++edge){reservation=monitor.rcMonitor;
        if(edge==0)reservation.top=reservation.bottom-100;else if(edge==1)reservation.bottom=reservation.top+100;
        else if(edge==2)reservation.right=reservation.left+100;else reservation.left=reservation.right-100;
        manager.changed();RECT actual{},overlap{};GetWindowRect(rail,&actual);
        if(IsWindowVisible(rail)&&IntersectRect(&overlap,&actual,&reservation))return 121;
    }
    reservation={};RECT obstacle=monitor.rcWork;manager.exclusions=[&](HMONITOR){return std::vector<RECT>{obstacle};};manager.changed();
    if(IsWindowVisible(rail))return 122;
    obstacle={};manager.changed();if(!IsWindowVisible(rail))return 123;
    MONITORINFO unchanged{sizeof(unchanged)};GetMonitorInfoW(display,&unchanged);if(!EqualRect(&monitor.rcWork,&unchanged.rcWork))return 124;
    st.perspective=false;manager.configure(st);rail=FindWindowW(L"TLauncher.Native.Stage",nullptr);
    ShowWindow(source,SW_RESTORE);ciPump(80);manager.changed();if(!manager.ciThumbnailCount())return 127;
    bool fullscreen=false;manager.fullscreen=[&](HMONITOR){return fullscreen;};manager.changed();
    fullscreen=true;manager.changed();if(IsWindowVisible(rail)||manager.ciThumbnailCount())return 125;
    fullscreen=false;manager.changed();if(!IsWindowVisible(rail))return 126;
    report<<"STAGE_FOUR_EDGE_RESERVED_GEOMETRY=PASS\nSTAGE_DYNAMIC_EXCLUSION_HIDE_RESUME=PASS\nSTAGE_FULLSCREEN_PREVIEW_RELEASE_RESUME=PASS\nWINDOWS_WORK_AREA_UNCHANGED=PASS\n";
    HWND second=CreateWindowW(L"STATIC",L"Stage second grouped window",WS_OVERLAPPEDWINDOW|WS_VISIBLE,380,200,560,360,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!second)return 128;DWORD secondPid=0;const DWORD secondTid=GetWindowThreadProcessId(second,&secondPid);HWND groupActivated=nullptr;
    manager.inventory=[&]{return std::vector<stage::Window>{{source,pid,tid,L"fixture",L"Cửa sổ thứ nhất",L""},{second,secondPid,secondTid,L"fixture",L"Cửa sổ thứ hai",L""}};};
    manager.activation=[&](HWND h){groupActivated=h;};manager.changed();
    const DWORD uiThread=GetCurrentThreadId();std::atomic<bool> menuClicked{false};
    std::thread menuDriver([&]{const ULONGLONG until=GetTickCount64()+4000;
        while(GetTickCount64()<until){HWND popup=FindWindowW(L"TLauncher.Native.Menu",nullptr);
            if(popup){RECT client{};GetClientRect(popup,&client);int x=(client.right-client.left)/2,y=client.bottom-int(16*vetro::dpiMonitor(MonitorFromWindow(popup,MONITOR_DEFAULTTONEAREST)));
                PostMessageW(popup,WM_MOUSEMOVE,0,MAKELPARAM(x,y));PostMessageW(popup,WM_LBUTTONUP,0,MAKELPARAM(x,y));menuClicked=true;return;}
            Sleep(10);
        }
        PostThreadMessageW(uiThread,WM_KEYDOWN,VK_ESCAPE,0);
    });
    SendMessageW(rail,WM_RBUTTONUP,0,MAKELPARAM(45,45));menuDriver.join();
    if(!menuClicked||groupActivated!=second||manager.ciSelected()!=second)return 129;
    manager.changed();if(manager.ciSelected()!=second)return 130;
    if(!phase3Snapshot(rail,evidence/L"phase3-stage-group-selection.bmp"))return 131;
    DestroyWindow(second);manager.inventory=[&]{return std::vector<stage::Window>{{source,pid,tid,L"fixture",L"Cửa sổ thứ nhất",L""}};};manager.changed();
    if(manager.ciSelected()!=source)return 132;
    report<<"STAGE_ACTUAL_GROUP_MENU_ACTIVATION_AND_PERSISTENCE=PASS\nSTAGE_DESTROYED_GROUP_MEMBER_FALLBACK=PASS\n";
    // Exercise actual DWM ownership across unrelated inventory changes. The
    // first live card must survive; only the new card gets an entrance fade.
    ciPump(240);manager.changed();const HTHUMBNAIL stableThumbnail=manager.ciThumbnail();
    if(!stableThumbnail||manager.ciAnimating()||manager.ciOpacity()!=1)return 166;
    HWND extra=CreateWindowW(L"STATIC",L"Stage animated independent card",WS_OVERLAPPEDWINDOW|WS_VISIBLE,460,250,500,320,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!extra)return 167;DWORD extraPid=0;DWORD extraTid=GetWindowThreadProcessId(extra,&extraPid);
    manager.inventory=[&]{std::vector<stage::Window> result{{source,pid,tid,L"fixture",L"Cửa sổ thứ nhất",L""}};
        if(IsWindow(extra))result.push_back({extra,extraPid,extraTid,L"other-fixture",L"Thẻ xuất hiện và biến mất",L""});return result;};
    manager.changed();if(!manager.ciAnimating()||manager.ciThumbnail()!=stableThumbnail)return 168;
    ciPump(260);if(manager.ciAnimating()||manager.ciThumbnail()!=stableThumbnail)return 169;
    const unsigned settledFrames=manager.ciAnimationFrames();ciPump(240);
    if(manager.ciAnimationFrames()!=settledFrames)return 170;
    // A completed asynchronous source snapshot is a legitimate repaint. Drain
    // that work before measuring the unchanged/settled inventory interval.
    for(int i=0;i<60&&manager.ciCapturePending();++i){ciPump(20);manager.changed();}
    if(manager.ciCapturePending())return 181;
    manager.changed();const unsigned idleRenderFrames=manager.ciRenderFrames();
    for(int i=0;i<4;++i){ciPump(100);manager.changed();}
    if(manager.ciRenderFrames()!=idleRenderFrames)return 180;
    if(!phase3Snapshot(rail,evidence/L"phase3-stage-animation-settled.bmp"))return 171;
    DestroyWindow(extra);manager.changed();
    if(manager.ciDeparting()!=1||!manager.ciAnimating()||manager.ciThumbnailCount()!=1)return 172;
    if(!phase3Snapshot(rail,evidence/L"phase3-stage-card-departing.bmp"))return 173;
    ciPump(260);if(manager.ciDeparting()||manager.ciAnimating())return 174;
    GetWindowPlacement(source,&after); // No animation route may mutate foreign placement.
    if(IsIconic(source))return 175;
    report<<"STAGE_CARD_ENTRANCE_EXIT_AND_IDLE_SETTLING=PASS\nSTAGE_UNCHANGED_INVENTORY_NO_REDRAW=PASS\nSTAGE_UNCHANGED_LIVE_THUMBNAIL_RETAINED=PASS\nSTAGE_REMOVED_SOURCE_THUMBNAIL_RELEASED=PASS\n";
    SetCursorPos(previousPointer.x,previousPointer.y);
    DestroyWindow(source);manager.changed();
    if(!manager.ciDeparting()||!manager.ciAnimating())return 176;
    st.enabled=false;manager.configure(st);ciPump(240);
    if(manager.railCount()||manager.snapshotBytes()||FindWindowW(L"TLauncher.Native.Stage",nullptr)||FindWindowW(L"TLauncher.Native.StagePeek",nullptr))return 72;
    report<<"STAGE_ACTUAL_NATIVE_WINDOW=PASS\nSTAGE_PEEK_FOCUS_PLACEMENT_UNCHANGED=PASS\nSTAGE_SHARED_ACTIVATION_CALLBACK=PASS\nSTAGE_DESTROY_AND_DISABLE_CLEANUP=PASS\n";
    gRicerca.elencoApp=[]{return std::vector<std::pair<std::wstring,std::wstring>>{{L"Notepad",L"notepad.exe"},{L"Ứng dụng",L"C:\\fixture\\tool.exe"}};};
    gRicerca.conOnda=[]{return false;};gRicerca.fullscreen=false;gRicerca.apri();ciPump(500);
    HWND search=FindWindowW(L"TLauncher.Native.Ricerca",nullptr);if(!search)return 73;
    SendMessageW(search,WM_CHAR,'n',0);SendMessageW(search,WM_CHAR,'o',0);ciPump(50);auto query=gRicerca.ciQuery();
    if(!phase3Snapshot(search,evidence/L"phase3-compact.bmp"))return 74;
    SendMessageW(search,WM_KEYDOWN,VK_F6,0);ciPump(80);if(!gRicerca.fullscreen||gRicerca.ciQuery()!=query)return 75;
    RECT full{};GetWindowRect(search,&full);RECT screen=vetro::areaPrincipale();if(!EqualRect(&full,&screen))return 76;
    if(!phase3Snapshot(search,evidence/L"phase3-fullscreen.bmp"))return 77;
    SendMessageW(search,WM_KEYDOWN,VK_F6,0);if(gRicerca.fullscreen||gRicerca.ciQuery()!=query)return 78;
    struct VietnameseSettingQuery{const wchar_t* query;const wchar_t* action;const wchar_t* title;};
    for(const auto& test:std::vector<VietnameseSettingQuery>{
        {L"độ phân giải",L"ms-settings:display",L"Màn hình"},
        {L"do phan giai",L"ms-settings:display",L"Màn hình"},
        {L"âm lượng",L"ms-settings:sound",L"Âm thanh"},
        {L"khong lam phien",L"ms-settings:notifications",L"Thông báo"},
        {L"trình duyệt",L"ms-settings:defaultapps",L"Ứng dụng mặc định"},
        {L"mui gio",L"ms-settings:dateandtime",L"Ngày giờ"},
        {L"bàn phím",L"ms-settings:typing",L"Bàn phím và nhập liệu"},
        {L"cài đặt tlauncher",L"*tlauncher-native",L"Cài đặt TLauncher"}}){
        const size_t previousLength=gRicerca.ciQuery().size();
        for(size_t i=0;i<previousLength;++i)SendMessageW(search,WM_KEYDOWN,VK_BACK,0);
        if(!gRicerca.ciQuery().empty())return 179;
        for(const wchar_t ch:std::wstring(test.query))SendMessageW(search,WM_CHAR,ch,0);
        if(!gRicerca.ciSetting(test.action,test.title)){
            auto utf8=[](const std::wstring& value){int size=WideCharToMultiByte(CP_UTF8,0,value.data(),int(value.size()),nullptr,0,nullptr,nullptr);std::string result(size,'\0');WideCharToMultiByte(CP_UTF8,0,value.data(),int(value.size()),result.data(),size,nullptr,nullptr);return result;};
            report<<"SPOTLIGHT_VI_QUERY requested="<<utf8(test.query)<<" actual="<<utf8(gRicerca.ciQuery())<<" language="<<testi::attuale()<<" expected="<<utf8(test.title)<<" action="<<utf8(test.action)<<" open="<<gRicerca.aperta()<<"\n";
            for(const auto& item:gRicerca.ciResultItems())report<<"SPOTLIGHT_VI_RESULT type="<<int(item.tipo)<<" title="<<utf8(item.titolo)<<" action="<<utf8(item.azione)<<"\n";
            report.flush();phase3Snapshot(search,evidence/L"phase3-spotlight-vietnamese-failure.bmp");return 177;
        }
    }
    if(!phase3Snapshot(search,evidence/L"phase3-spotlight-vietnamese-settings.bmp"))return 178;
    report<<"SPOTLIGHT_ACTUAL_VIETNAMESE_SETTINGS_SYNONYMS_AND_ACCENT_FOLDING=PASS\n";
    gRicerca.chiudi();ciPump(250);
    gRicerca.elencoApp=[]{std::vector<std::pair<std::wstring,std::wstring>> result;for(int i=0;i<20;i++)result.push_back({L"Tool "+std::to_wstring(i),L"C:\\fixture\\tool"+std::to_wstring(i)+L".exe"});return result;};
    gRicerca.fullscreen=true;gRicerca.apri();ciPump(450);search=FindWindowW(L"TLauncher.Native.Ricerca",nullptr);
    SendMessageW(search,WM_CHAR,'t',0);SendMessageW(search,WM_CHAR,'o',0);ciPump(50);
    for(int i=0;i<8;i++)SendMessageW(search,WM_KEYDOWN,VK_RIGHT,0);const auto selected=gRicerca.ciSelection();
    SendMessageW(search,WM_KEYDOWN,VK_F6,0);if(gRicerca.ciSelection()!=selected)return 88;
    SendMessageW(search,WM_KEYDOWN,VK_F6,0);if(gRicerca.ciSelection()!=selected)return 89;
    wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);std::wstring dropPath=executable;
    HGLOBAL drop=GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT,sizeof(DROPFILES)+(dropPath.size()+2)*sizeof(wchar_t));
    if(!drop)return 90;auto data=static_cast<DROPFILES*>(GlobalLock(drop));data->pFiles=sizeof(DROPFILES);data->fWide=TRUE;
    memcpy(reinterpret_cast<BYTE*>(data)+sizeof(DROPFILES),dropPath.c_str(),(dropPath.size()+1)*sizeof(wchar_t));GlobalUnlock(drop);
    SendMessageW(search,WM_DROPFILES,reinterpret_cast<WPARAM>(drop),0);if(!appcatalog::get(dropPath).manual)return 91;
    SendMessageW(search,WM_KEYDOWN,VK_ESCAPE,0);ciPump(220);if(gRicerca.aperta())return 92;
    report<<"SPOTLIGHT_F6_LOGICAL_SELECTION_PRESERVED=PASS\nSPOTLIGHT_NATIVE_EXE_DROP_METADATA=PASS\nSPOTLIGHT_ESCAPE_DISMISS=PASS\n";
    const auto iconPath=evidence/L"phase3-custom-icon.bmp";
    gRicerca.fullscreen=true;
    gRicerca.apri();ciPump(300);
    search=FindWindowW(L"TLauncher.Native.Ricerca",nullptr);
    if(!search||!phase3Snapshot(search,iconPath))return 184;
    gRicerca.chiudi();ciPump(260);
    auto iconMeta=appcatalog::get(dropPath);
    iconMeta.icon=iconPath.wstring();appcatalog::save(dropPath,iconMeta);
    gRicerca.elencoApp=[dropPath]{return std::vector<std::pair<std::wstring,std::wstring>>{{L"Fixture icon",dropPath}};};
    gRicerca.ferma();gRicerca.prefetch();
    for(int i=0;i<300&&!gRicerca.ciWorkerIdle();++i)ciPump(10);
    gRicerca.apri();
    search=FindWindowW(L"TLauncher.Native.Ricerca",nullptr);
    const auto firstBitmap=gRicerca.ciBitmap(iconPath.wstring());
    if(!search||!firstBitmap)return 185;
    ciPump(480);
    if(!phase3Snapshot(search,evidence/L"phase3-spotlight-cached-icon.bmp"))return 186;
    gRicerca.chiudi();ciPump(260);
    gRicerca.apri();
    if(gRicerca.ciBitmap(iconPath.wstring())!=firstBitmap)return 187;
    SendMessageW(search,WM_CHAR,'f',0);
    SendMessageW(search,WM_KEYDOWN,VK_F6,0);
    if(gRicerca.ciBitmap(iconPath.wstring())!=firstBitmap)return 188;
    gRicerca.chiudi();ciPump(260);
    report<<"SPOTLIGHT_PREFETCH_FIRST_FRAME_AND_REOPEN_BITMAP_REUSE=PASS\n";
    // Closing invalidates outstanding file/icon work. A fresh opening starts
    // with no stale query; disabling releases HWND, timers, fonts and bitmaps.
    gRicerca.ferma();
    if(FindWindowW(L"TLauncher.Native.Ricerca",nullptr)||gRicerca.ciBitmapCount()||gRicerca.ciPendingWork())return 94;
    gRicerca.fullscreen=false;gRicerca.apri();ciPump(80);search=FindWindowW(L"TLauncher.Native.Ricerca",nullptr);
    if(!search||!gRicerca.ciQuery().empty())return 95;
    SendMessageW(search,WM_CHAR,'t',0);SendMessageW(search,WM_CHAR,'o',0);ciPump(190);gRicerca.ferma();
    for(int i=0;i<400&&!gRicerca.ciWorkerIdle();++i)ciPump(10);
    if(!gRicerca.ciWorkerIdle()||gRicerca.ciPendingWork()||gRicerca.ciBitmapCount()||FindWindowW(L"TLauncher.Native.Ricerca",nullptr))return 96;
    for(int i=0;i<3;++i){gRicerca.apri();ciPump(40);gRicerca.ferma();}
    if(FindWindowW(L"TLauncher.Native.Ricerca",nullptr)||gRicerca.ciPendingWork())return 97;
    report<<"SPOTLIGHT_DISABLE_NATIVE_RESOURCES_RELEASED=PASS\nSPOTLIGHT_WORKER_DRAINS_AND_EXITS=PASS\nSPOTLIGHT_REOPEN_AFTER_CANCELLATION=PASS\n";
    report<<"SPOTLIGHT_NATIVE_COMPACT_FULLSCREEN=PASS\nF6_QUERY_PRESERVED=PASS\nOWNER_VISUAL_ACCEPTANCE=PENDING\n";
    return 0;
}
