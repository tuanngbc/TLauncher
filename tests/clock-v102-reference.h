#pragma once
// Literal layout/paint port of v1.0.2 src/widget.rs::layout/text/paint.
// Kept outside production; compares frozen GDI pixels rather than font filenames.
namespace clockreference {
struct Image {SIZE size{};std::vector<unsigned> pixels;};
inline void save(const Image& image,const std::filesystem::path& path){
    BITMAPFILEHEADER header{};BITMAPINFOHEADER info{};
    info.biSize=sizeof(info);info.biWidth=image.size.cx;info.biHeight=-image.size.cy;
    info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;
    header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(info);
    header.bfSize=header.bfOffBits+DWORD(image.pixels.size()*4);
    std::ofstream out(path,std::ios::binary);out.write((char*)&header,sizeof(header));
    out.write((char*)&info,sizeof(info));out.write((char*)image.pixels.data(),image.pixels.size()*4);
}
inline Image render(float scale,const SYSTEMTIME& time,bool vietnamese){
    HDC dc=CreateCompatibleDC(nullptr);
    wchar_t hour[40]{},day[128]{},date[128]{};
    GetTimeFormatEx(L"en-US",0,&time,L"HH:mm",hour,40);
    GetDateFormatEx(vietnamese?L"vi-VN":L"en-US",0,&time,L"dddd",day,128,nullptr);
    if(vietnamese)GetDateFormatEx(L"vi-VN",0,&time,L"d MMMM yyyy",date,128,nullptr);
    else {
        wchar_t month[48]{};GetDateFormatEx(L"en-US",0,&time,L"MMMM",month,48,nullptr);
        const unsigned d=time.wDay;
        const wchar_t* suffix=d>=11&&d<=13?L"th":d%10==1?L"st":d%10==2?L"nd":d%10==3?L"rd":L"th";
        swprintf(date,128,L"%u%ls %ls %u",d,suffix,month,unsigned(time.wYear));
    }
    CharUpperBuffW(date,DWORD(wcslen(date)));
    struct Run{const wchar_t* value;const wchar_t* face;int height,base;COLORREF color;RECT rect{};};
    std::array<Run,3> runs{{
        {hour,L"Steelfish Rg",int(100*scale),0,RGB(185,231,245)},
        {date,L"Steelfish Rg",int(35*scale),150,RGB(185,231,245)},
        {day,L"la Compagnie des Ombres",int(85*scale),70,RGB(220,247,255)}
    }};
    RECT bounds{0,0,LONG(320*scale),LONG(230*scale)};
    auto font=[](const Run& run){return CreateFontW(run.height,0,0,0,400,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,run.face);};
    for(auto& run:runs){
        auto f=font(run);auto old=SelectObject(dc,f);SIZE extent{};
        GetTextExtentPoint32W(dc,run.value,int(wcslen(run.value)),&extent);
        SelectObject(dc,old);DeleteObject(f);
        const LONG half=(std::max(1L,extent.cx)+1)/2,top=LONG(run.base*scale);
        run.rect={120-half,top,120+half,top+run.height+8};
        const int pad=std::max(8,run.height);
        bounds.left=std::min(bounds.left,run.rect.left-pad);bounds.right=std::max(bounds.right,run.rect.right+pad);
        bounds.top=std::min(bounds.top,run.rect.top-pad);bounds.bottom=std::max(bounds.bottom,run.rect.bottom+pad);
    }
    Image image;image.size={bounds.right-bounds.left,bounds.bottom-bounds.top};
    BITMAPINFO info{};info.bmiHeader={sizeof(BITMAPINFOHEADER),image.size.cx,-image.size.cy,1,32,BI_RGB,0,0,0,0,0};
    void* data=nullptr;auto bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&data,nullptr,0);
    auto oldBitmap=SelectObject(dc,bitmap);std::fill_n((unsigned*)data,size_t(image.size.cx)*image.size.cy,0u);SetBkMode(dc,TRANSPARENT);
    for(auto& run:runs){
        auto f=font(run);auto old=SelectObject(dc,f);SetTextColor(dc,run.color);
        RECT rect=run.rect;OffsetRect(&rect,-bounds.left,-bounds.top);
        DrawTextW(dc,run.value,-1,&rect,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOCLIP|DT_NOPREFIX);
        SelectObject(dc,old);DeleteObject(f);
    }
    GdiFlush();auto* pixels=(unsigned*)data;
    image.pixels.assign(pixels,pixels+size_t(image.size.cx)*image.size.cy);
    for(auto& pixel:image.pixels){unsigned rgb=pixel&0xffffff;pixel=rgb|(std::max({rgb&255,(rgb>>8)&255,rgb>>16})<<24);}
    SelectObject(dc,oldBitmap);DeleteObject(bitmap);DeleteDC(dc);return image;
}
}
