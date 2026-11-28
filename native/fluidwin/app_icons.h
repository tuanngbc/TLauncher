// Shared pixel cache. Extraction only runs in the existing background workers.
// UI callers use peek(); D2D bitmaps remain owned by their render target.
#pragma once
#include "tlauncher_identity.h"
#include <vector>
#include <unordered_map>
#include <mutex>
#include <fstream>
#include <memory>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <wincodec.h>

namespace appicons {
inline constexpr wchar_t normalizationVersion[]=L"optical-v2|";
struct Pixels {int w=0,h=0;std::vector<UINT> bgra;};
struct Entry {std::shared_ptr<const Pixels> pixels;ULONGLONG used=0;};
inline std::mutex mutex;
inline std::unordered_map<std::wstring,Entry> memory;
inline constexpr size_t budget=24*1024*1024;
inline std::unordered_map<std::wstring,std::wstring> aliases;
inline std::unordered_map<std::wstring,ULONGLONG> failures;
inline std::wstring requestKey(const std::wstring& source,int size){
    std::wstring k=source;for(auto& c:k)c=(wchar_t)towlower(c);return std::wstring(normalizationVersion)+k+L"|"+std::to_wstring(size);
}
inline bool canRequest(const std::wstring& source,int size){
    std::lock_guard<std::mutex> lock(mutex);auto i=failures.find(requestKey(source,size));
    if(i==failures.end())return true;if(GetTickCount64()<i->second)return false;failures.erase(i);return true;
}
inline void failed(const std::wstring& source,int size){
    std::lock_guard<std::mutex> lock(mutex);if(failures.size()>2048)failures.clear();failures[requestKey(source,size)]=GetTickCount64()+30000;
}
inline std::wstring key(const std::wstring& source,int size) {
    std::wstring s=source;for(auto& c:s)c=(wchar_t)towlower(c);
    WIN32_FILE_ATTRIBUTE_DATA a{};
    if(GetFileAttributesExW(source.c_str(),GetFileExInfoStandard,&a))
        s+=L"|"+std::to_wstring(a.ftLastWriteTime.dwHighDateTime)+L":"+std::to_wstring(a.ftLastWriteTime.dwLowDateTime)+L":"+std::to_wstring(a.nFileSizeLow);
    return std::wstring(normalizationVersion)+s+L"|"+std::to_wstring(size);
}
inline uint64_t hash(const void* data,size_t n) {
    uint64_t h=14695981039346656037ull;auto b=(const unsigned char*)data;
    for(size_t i=0;i<n;++i){h^=b[i];h*=1099511628211ull;}return h;
}
inline std::filesystem::path path(const std::wstring& k) {
    return std::filesystem::path(tlauncher::profile())/L"icon-cache"/(std::to_wstring(hash(k.data(),k.size()*sizeof(wchar_t)))+L".bgra");
}
inline void remember(const std::wstring& alias,const std::wstring& k,std::shared_ptr<const Pixels> p) {
    std::lock_guard<std::mutex> lock(mutex);aliases[alias]=k;failures.erase(alias);memory[k]={std::move(p),GetTickCount64()};
    size_t bytes=0;for(auto& e:memory)bytes+=e.second.pixels->bgra.size()*4;
    while(bytes>budget&&!memory.empty()) {auto old=std::min_element(memory.begin(),memory.end(),[](auto& a,auto& b){return a.second.used<b.second.used;});bytes-=old->second.pixels->bgra.size()*4;const auto evicted=old->first;memory.erase(old);
        for(auto i=aliases.begin();i!=aliases.end();)if(i->second==evicted)i=aliases.erase(i);else ++i;
    }
}
inline std::shared_ptr<const Pixels> peek(const std::wstring& source,int size) {
    // No filesystem or Shell calls on a draw path. Workers validate file revision.
    const auto alias=requestKey(source,size);std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);
    if(!lock.owns_lock())return {};
    auto a=aliases.find(alias);if(a==aliases.end())return {};
    auto i=memory.find(a->second);if(i==memory.end())return {};i->second.used=GetTickCount64();return i->second.pixels;
}
inline bool customImage(const std::wstring& source){auto ext=std::filesystem::path(source).extension().wstring();for(auto&c:ext)c=(wchar_t)towlower(c);return ext==L".png"||ext==L".jpg"||ext==L".jpeg"||ext==L".bmp"||ext==L".ico";}
inline bool decodeImage(const std::wstring& source,int size,int& w,int& h,std::vector<UINT>& pixels){
    // Decode user-selected artwork; asking Shell for ICONONLY yields the image
    // application's generic icon rather than the selected custom bitmap.
    IWICImagingFactory* factory=nullptr;IWICBitmapDecoder* decoder=nullptr;IWICBitmapFrameDecode* frame=nullptr;IWICBitmapScaler* scaler=nullptr;IWICFormatConverter* converter=nullptr;
    HRESULT hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory));
    if(SUCCEEDED(hr))hr=factory->CreateDecoderFromFilename(source.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnDemand,&decoder);
    if(SUCCEEDED(hr))hr=decoder->GetFrame(0,&frame);UINT width=0,height=0;if(SUCCEEDED(hr))hr=frame->GetSize(&width,&height);
    if(!width||!height||uint64_t(width)*height>64*1024*1024)hr=E_FAIL;
    if(SUCCEEDED(hr)){const double scale=std::min(double(size)/width,double(size)/height);w=std::max(1,int(width*scale));h=std::max(1,int(height*scale));hr=factory->CreateBitmapScaler(&scaler);}
    if(SUCCEEDED(hr))hr=scaler->Initialize(frame,w,h,WICBitmapInterpolationModeFant);
    if(SUCCEEDED(hr))hr=factory->CreateFormatConverter(&converter);
    if(SUCCEEDED(hr))hr=converter->Initialize(scaler,GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom);
    if(SUCCEEDED(hr)){std::vector<UINT> fitted(size_t(w)*h);hr=converter->CopyPixels(nullptr,w*4,UINT(fitted.size()*4),reinterpret_cast<BYTE*>(fitted.data()));
        if(SUCCEEDED(hr)){pixels.assign(size_t(size)*size,0);const int left=(size-w)/2,top=(size-h)/2;for(int y=0;y<h;y++)std::copy_n(fitted.data()+size_t(y)*w,w,pixels.data()+size_t(y+top)*size+left);w=h=size;}}
    if(converter)converter->Release();if(scaler)scaler->Release();if(frame)frame->Release();if(decoder)decoder->Release();if(factory)factory->Release();return SUCCEEDED(hr);
}
inline void normalize(Pixels& image,int size){
    int left=image.w,top=image.h,right=0,bottom=0;
    for(int y=0;y<image.h;++y)for(int x=0;x<image.w;++x){
        if((image.bgra[size_t(y)*image.w+x]>>24)<24)continue;
        left=std::min(left,x);top=std::min(top,y);right=std::max(right,x+1);bottom=std::max(bottom,y+1);
    }
    if(right<=left||bottom<=top)return;
    const int width=right-left,height=bottom-top;
    const double scale=std::min(4.0,double(size)*.90/std::max(width,height));
    // Bounds define the optical scale; retain every source pixel (including halo)
    // when it fits, falling back to fitting the complete bitmap when necessary.
    int allLeft=image.w,allTop=image.h,allRight=0,allBottom=0;
    for(int y=0;y<image.h;++y)for(int x=0;x<image.w;++x)if(image.bgra[size_t(y)*image.w+x]>>24){
        allLeft=std::min(allLeft,x);allTop=std::min(allTop,y);allRight=std::max(allRight,x+1);allBottom=std::max(allBottom,y+1);
    }
    left=allLeft;top=allTop;right=allRight;bottom=allBottom;
    const double fit=std::min(scale,double(size)/std::max(right-left,bottom-top));
    const int w=std::max(1,int(std::lround((right-left)*fit))),h=std::max(1,int(std::lround((bottom-top)*fit)));
    std::vector<UINT> result(size_t(size)*size,0);
    const auto sample=[&](int x,int y){return image.bgra[size_t(y)*image.w+x];};
    for(int y=0;y<h;++y)for(int x=0;x<w;++x){
        const double px=std::clamp(left+(x+.5)*(right-left)/w-.5,double(left),double(right-1));
        const double py=std::clamp(top+(y+.5)*(bottom-top)/h-.5,double(top),double(bottom-1));
        const int a=int(px),b=int(py),c=std::min(a+1,right-1),d=std::min(b+1,bottom-1);
        const double fx=px-a,fy=py-b;
        const UINT p=sample(a,b),q=sample(c,b),r=sample(a,d),s=sample(c,d);
        UINT value=0;
        for(int shift=0;shift<32;shift+=8){
            const double u=((p>>shift)&255)*(1-fx)+((q>>shift)&255)*fx;
            const double v=((r>>shift)&255)*(1-fx)+((s>>shift)&255)*fx;
            value|=UINT(std::lround(u*(1-fy)+v*fy))<<shift;
        }
        result[size_t(y+(size-h)/2)*size+x+(size-w)/2]=value;
    }
    image={size,size,std::move(result)};
}
struct Header {uint32_t magic=0x31494354,w=0,h=0,reserved=0;uint64_t checksum=0;};
template<class Extract> bool load(const std::wstring& source,int size,int& w,int& h,std::vector<UINT>& pixels,Extract extract,std::shared_ptr<const Pixels>* shared=nullptr) {
    if(size<16||size>512||!canRequest(source,size))return false;
    const auto k=key(source,size);
    {std::lock_guard<std::mutex> lock(mutex);auto i=memory.find(k);if(i!=memory.end()){w=i->second.pixels->w;h=i->second.pixels->h;if(shared)*shared=i->second.pixels;else pixels=i->second.pixels->bgra;i->second.used=GetTickCount64();aliases[requestKey(source,size)]=k;return true;}}
    auto p=std::make_shared<Pixels>();
    Header head;bool valid=false;
    {std::ifstream in(path(k),std::ios::binary);if(in.read((char*)&head,sizeof(head))&&head.magic==0x31494354&&head.w>0&&head.h>0&&head.w<=512&&head.h<=512){p->w=head.w;p->h=head.h;p->bgra.resize(size_t(head.w)*head.h);valid=bool(in.read((char*)p->bgra.data(),p->bgra.size()*4))&&hash(p->bgra.data(),p->bgra.size()*4)==head.checksum;}}
    if(!valid) {
        p->bgra.clear();if(!(customImage(source)?decodeImage(source,size,p->w,p->h,p->bgra):extract(source,size,p->w,p->h,p->bgra))||p->w<=0||p->h<=0||p->w>512||p->h>512||p->bgra.size()!=size_t(p->w)*p->h){failed(source,size);return false;}
        normalize(*p,size);
        std::error_code ec;const auto dest=path(k);std::filesystem::create_directories(dest.parent_path(),ec);
        // Bound persistent storage too; stale entries are expendable cache, not user assets.
        size_t total=0;std::vector<std::filesystem::directory_entry> files;
        for(std::filesystem::directory_iterator it(dest.parent_path(),ec),end;!ec&&it!=end;it.increment(ec))if(it->is_regular_file(ec)&&it->path().extension()==L".bgra"){total+=it->file_size(ec);files.push_back(*it);}
        if(total>64*1024*1024){std::sort(files.begin(),files.end(),[&](auto& a,auto& b){return a.last_write_time(ec)<b.last_write_time(ec);});for(auto& f:files){if(total<=48*1024*1024)break;auto n=f.file_size(ec);if(std::filesystem::remove(f.path(),ec))total-=n;}}
        auto temp=dest;temp+=L"."+std::to_wstring(GetCurrentThreadId())+L".tmp";
        head.w=p->w;head.h=p->h;head.checksum=hash(p->bgra.data(),p->bgra.size()*4);
        {std::ofstream out(temp,std::ios::binary);out.write((char*)&head,sizeof(head));out.write((char*)p->bgra.data(),p->bgra.size()*4);out.flush();if(!out){out.close();std::filesystem::remove(temp,ec);}}
        MoveFileExW(temp.c_str(),dest.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
    }
    w=p->w;h=p->h;if(shared)*shared=p;else pixels=p->bgra;remember(requestKey(source,size),k,p);return true;
}
} // namespace appicons
