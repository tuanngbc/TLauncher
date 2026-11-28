// Hosted only. Exercise the actual migrated Settings/Notch source, not mocks.
#define TLAUNCHER_NATIVE_CI
#define wWinMain tlauncher_application_entry
#include "../native/fluidwin/fluidwin.cpp"
#undef wWinMain
#include <fstream>
#include <filesystem>
static void ciPump(int ms) {
    const ULONGLONG until=GetTickCount64()+ms;
    do {MSG m;while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}Sleep(5);}while(GetTickCount64()<until);
}
#include "native-phase3-contract.h"
int main(int argc,char** argv) {
    AddVectoredExceptionHandler(1,[](EXCEPTION_POINTERS* info)->LONG{
        const DWORD code=info->ExceptionRecord->ExceptionCode;
        if(code==EXCEPTION_ACCESS_VIOLATION||code==EXCEPTION_STACK_OVERFLOW||code==0xe06d7363){
            const auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
            fprintf(stderr,"NATIVE_EXCEPTION code=%08lx address=%p offset=%llx\n",code,info->ExceptionRecord->ExceptionAddress,
                (unsigned long long)(reinterpret_cast<uintptr_t>(info->ExceptionRecord->ExceptionAddress)-base));
            void* frames[32]{};const auto count=CaptureStackBackTrace(0,32,frames,nullptr);
            for(USHORT i=0;i<count;++i)fprintf(stderr,"NATIVE_FRAME %llx\n",(unsigned long long)(reinterpret_cast<uintptr_t>(frames[i])-base));
            fflush(stderr);
        }return EXCEPTION_CONTINUE_SEARCH;
    });
    if((argc!=2&&argc!=3)||!tlauncher::validateTrial())return 10;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    OleInitialize(nullptr);
    std::ofstream report(std::filesystem::path(argv[1])/"native-contract.txt");
    if(wcsstr(tlauncher::registry(),L"FluidWin")||!wcsstr(tlauncher::registry(),L"TLauncher\\NativeBase\\Profiles"))return 11;
    impostazioni::carica();
    if(argc==3&&!strcmp(argv[2],"--verify-migrated")){
        const auto&s=impostazioni::valori();wallpaper::Store store;const auto c=store.load();
        if(testi::attuale()!=testi::VIETNAMITA||s.notchEnabled||s.dockVisibility!=2||s.stage.height!=174||s.stage.peekDelay!=750||s.spotlightMode!=2)return 83;
        if(s.spotlightShortcut!=shortcuts::Key{MOD_CONTROL|MOD_ALT,VK_F22}||s.stageShortcut!=shortcuts::Key{MOD_ALT|MOD_SHIFT,'S'}||s.stageShortcutEnabled)return 117;
        if(!c.enabled||c.scene!=wallpaper::sceneIndex("lake")||c.fps!=29||c.layout!=wallpaper::Layout::Panorama)return 84;
        if(!s.stage.enabled||c.resolution!=.75f||c.pauseCovered||c.weather!=wallpaper::WeatherMode::Offline||c.clockLighting||c.manualHour!=17.25f||std::abs(c.wind-1.4f)>.001f||wallpaper::parameter(c,c.scene,"depth")!=321)return 118;
        if(c.displays.empty())return 119;for(const auto& entry:c.displays){const auto& a=entry.second;if(!a.enabled||a.scene!=wallpaper::sceneIndex("aquarium")||a.sceneParams.at("aquarium").at("fish")!=6)return 120;}
        fprintf(stderr,"MIGRATED_SETTINGS_OPEN_BEGIN\n");fflush(stderr);
        gImpostazioni.apri();fprintf(stderr,"MIGRATED_SETTINGS_OPEN_RETURNED\n");fflush(stderr);ciPump(350);if(!FindWindowW(L"TLauncher.Native.Impostazioni",nullptr))return 85;
        if(!phase3Snapshot(FindWindowW(L"TLauncher.Native.Impostazioni",nullptr),std::filesystem::path(argv[1])/L"phase3-migrated-vietnamese-settings.bmp"))return 86;
        report<<"MIGRATED_CUSTOM_NATIVE_SETTINGS_READ=PASS\nMIGRATED_WALLPAPER_READ=PASS\nVIETNAMESE_NATIVE_SETTINGS_RENDER=PASS\n";report.close();ExitProcess(0);
    }
    impostazioni::valori().notchEnabled=false;impostazioni::valori().notch=false;impostazioni::salva();
    impostazioni::valori().notchEnabled=true;impostazioni::carica();
    if(impostazioni::valori().notchEnabled||impostazioni::valori().notch)return 12;
    impostazioni::impostaAvvio(true);
    if(impostazioni::avvioAttivo())return 13;
    report<<"PROFILE_NAMESPACE=PASS\nNOTCH_OFF_PERSISTENCE=PASS\nSTARTUP_WRITE_BLOCK=PASS\n";
    {
        notch::Notch n;
        if(!n.crea(GetModuleHandleW(nullptr)))return 14;
        n.posto({0,0,2560,1080},1);n.testi(L"12:34",L"Thu 26");n.mostra(true);ciPump(100);
        n.abilita(false);ciPump(100);
        n.volume(0.5f,false);ciPump(80);
        n.enable(false);
        if(!n.ciReleased()||FindWindowW(L"TLauncher.Native.Notch",nullptr))return 15;
        const DWORD gdi=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
        for(int i=0;i<4;i++) {
            n.enable(true);n.abilita(i%2==0);ciPump(40);
            if(!FindWindowW(L"TLauncher.Native.Notch",nullptr))return 16;
            n.enable(false);n.posto({-1080,0,0,1920},1.25f);n.testi(L"12:35",L"Fri 27");
            if(!n.ciReleased())return 17;
        }
        if(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)>gdi+2)return 18;
        n.mostra(false);
        report<<"NOTCH_TRUE_DISABLE=PASS\nNOTCH_LIVE_REENABLE=PASS\nNOTCH_FIXED_AND_ACTIVITY=PASS\nNOTCH_GDI_RELEASE=PASS\n";
    }
    if(int result=phase3Contracts(report,std::filesystem::path(argv[1])))return result;
    gImpostazioni.apri();ciPump(350);
    if(!FindWindowW(L"TLauncher.Native.Impostazioni",nullptr))return 19;
    const HWND settings=FindWindowW(L"TLauncher.Native.Impostazioni",nullptr);
    const ULONGLONG readyUntil=GetTickCount64()+3000;while(!gImpostazioni.ciReady()&&GetTickCount64()<readyUntil)ciPump(20);
    if(!gImpostazioni.ciReady())return 140;
    for(int language:{testi::INGLESE,testi::VIETNAMITA,testi::FRANCESE,testi::VIETNAMITA}){
        POINT point{};if(!gImpostazioni.ciPoint(100+language,point))return 141;
        SendMessageW(settings,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(point.x,point.y));
        SendMessageW(settings,WM_LBUTTONUP,0,MAKELPARAM(point.x,point.y));ciPump(90);
        const auto fontLocale=gImpostazioni.ciFontLocale();report<<"DIRECTWRITE_LOCALE="<<std::string(fontLocale.begin(),fontLocale.end())<<"\n";
        report<<"LANGUAGE_CLICK requested="<<language<<" actual="<<testi::attuale()<<" formats_rebuilt="<<gImpostazioni.ciLocale()<<" ready="<<gImpostazioni.ciReady()<<" point="<<point.x<<","<<point.y<<"\n";report.flush();
        if(testi::attuale()!=language||!gImpostazioni.ciLocale()){phase3Snapshot(settings,std::filesystem::path(argv[1])/L"phase3-language-failure.bmp");return 142;}
    }
    impostazioni::carica();if(testi::attuale()!=testi::VIETNAMITA)return 123;
    if(!phase3Snapshot(settings,std::filesystem::path(argv[1])/L"phase3-live-vietnamese-settings.bmp"))return 124;
    report<<"NATIVE_SETTINGS_WINDOW=PASS\nLIVE_LANGUAGE_CHANGE_FORMAT_LOCALE_AND_PERSISTENCE=PASS\nOWNER_VISUAL_ACCEPTANCE=PENDING\n";
    SendMessageW(settings,WM_CLOSE,0,0);ciPump(500);
    if(int result=phase3DockContracts(report,std::filesystem::path(argv[1])))return result;
    report.close();
    // Upstream exits without static teardown after workers; retain that policy.
    ExitProcess(0);
}
