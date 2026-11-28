//! Bounded desktop recorder: one reusable capture buffer, one NV12 frame,
//! synchronous Media Foundation writes (backpressure), optional WASAPI loopback.
use super::screen::{self,Area,Grab};
use crate::desktop::wide;
use std::cell::RefCell;
use std::collections::VecDeque;
use std::sync::{Arc,atomic::{AtomicBool,Ordering},mpsc};
use std::time::{Duration,Instant};
use windows::Win32::Media::Audio::*;
use windows::Win32::Media::MediaFoundation::*;
use windows::Win32::System::Com::*;
use windows::core::PCWSTR;
use windows_sys::Win32::Foundation::*;
use windows_sys::Win32::Graphics::Gdi::*;
use windows_sys::Win32::UI::Input::KeyboardAndMouse::*;
use windows_sys::Win32::UI::WindowsAndMessaging::*;

struct Loopback {client:IAudioClient,capture:IAudioCaptureClient,queue:VecDeque<u8>}
impl Loopback {
    unsafe fn new()->windows::core::Result<Self>{
        let enumerator:IMMDeviceEnumerator=CoCreateInstance(&MMDeviceEnumerator,None,CLSCTX_ALL)?;
        let device=enumerator.GetDefaultAudioEndpoint(eRender,eConsole)?;
        let client:IAudioClient=device.Activate(CLSCTX_ALL,None)?;
        let format=WAVEFORMATEX{wFormatTag:1,nChannels:2,nSamplesPerSec:48000,nAvgBytesPerSec:192000,nBlockAlign:4,wBitsPerSample:16,cbSize:0};
        client.Initialize(AUDCLNT_SHAREMODE_SHARED,AUDCLNT_STREAMFLAGS_LOOPBACK|AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM|AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,1_000_000,0,&format,None)?;
        let capture=client.GetService::<IAudioCaptureClient>()?;client.Start()?;
        Ok(Self{client,capture,queue:VecDeque::with_capacity(48000)})
    }
    unsafe fn poll(&mut self,discard:bool)->windows::core::Result<()>{
        while self.capture.GetNextPacketSize()?>0 {
            let mut data=std::ptr::null_mut();let mut frames=0;let mut flags=0;
            self.capture.GetBuffer(&mut data,&mut frames,&mut flags,None,None)?;
            if !discard{let n=frames as usize*4;if flags&AUDCLNT_BUFFERFLAGS_SILENT.0 as u32!=0||data.is_null(){self.queue.extend(std::iter::repeat_n(0,n));}else{self.queue.extend(std::slice::from_raw_parts(data,n));}}
            self.capture.ReleaseBuffer(frames)?;
        }
        if discard{self.queue.clear();}
        // Bound audio backlog to 250 ms, including a slow encoder/device stall.
        if self.queue.len()>48000 {let extra=(self.queue.len()-48000)/4*4;self.queue.drain(..extra);}
        Ok(())
    }
    fn take(&mut self,frames:usize)->Vec<u8>{(0..frames*4).map(|_|self.queue.pop_front().unwrap_or(0)).collect()}
}
impl Drop for Loopback{fn drop(&mut self){unsafe{let _=self.client.Stop();}}}

fn packed_pair(a:u32,b:u32)->u64{((a as u64)<<32)|b as u64}
unsafe fn video_type(area:Area,fps:u32,encoded:bool)->windows::core::Result<IMFMediaType>{
    let t=MFCreateMediaType()?;t.SetGUID(&MF_MT_MAJOR_TYPE,&MFMediaType_Video)?;t.SetGUID(&MF_MT_SUBTYPE,if encoded{&MFVideoFormat_H264}else{&MFVideoFormat_NV12})?;
    t.SetUINT64(&MF_MT_FRAME_SIZE,packed_pair(area.w as u32,area.h as u32))?;t.SetUINT64(&MF_MT_FRAME_RATE,packed_pair(fps,1))?;t.SetUINT64(&MF_MT_PIXEL_ASPECT_RATIO,packed_pair(1,1))?;t.SetUINT32(&MF_MT_INTERLACE_MODE,MFVideoInterlace_Progressive.0 as u32)?;
    t.SetUINT32(&MF_MT_YUV_MATRIX,MFVideoTransferMatrix_BT709.0 as u32)?;
    if encoded{let rate=((area.w as u64*area.h as u64*fps as u64)/8).clamp(2_000_000,24_000_000)as u32;t.SetUINT32(&MF_MT_AVG_BITRATE,rate)?;}
    Ok(t)
}
unsafe fn audio_type(encoded:bool)->windows::core::Result<IMFMediaType>{let t=MFCreateMediaType()?;t.SetGUID(&MF_MT_MAJOR_TYPE,&MFMediaType_Audio)?;t.SetGUID(&MF_MT_SUBTYPE,if encoded{&MFAudioFormat_AAC}else{&MFAudioFormat_PCM})?;t.SetUINT32(&MF_MT_AUDIO_NUM_CHANNELS,2)?;t.SetUINT32(&MF_MT_AUDIO_SAMPLES_PER_SECOND,48000)?;t.SetUINT32(&MF_MT_AUDIO_BITS_PER_SAMPLE,16)?;if encoded{t.SetUINT32(&MF_MT_AUDIO_AVG_BYTES_PER_SECOND,24000)?;}else{t.SetUINT32(&MF_MT_AUDIO_BLOCK_ALIGNMENT,4)?;t.SetUINT32(&MF_MT_AUDIO_AVG_BYTES_PER_SECOND,192000)?;}Ok(t)}
unsafe fn sample(bytes:&[u8],time:i64,duration:i64)->windows::core::Result<IMFSample>{
    let buffer=MFCreateMemoryBuffer(bytes.len()as u32)?;let mut ptr=std::ptr::null_mut();buffer.Lock(&mut ptr,None,None)?;std::ptr::copy_nonoverlapping(bytes.as_ptr(),ptr,bytes.len());buffer.Unlock()?;buffer.SetCurrentLength(bytes.len()as u32)?;let s=MFCreateSample()?;s.AddBuffer(&buffer)?;s.SetSampleTime(time)?;s.SetSampleDuration(duration)?;Ok(s)
}
// SDR BT.709 limited-range, top-down NV12. No bitmap orientation ambiguity.
fn nv12(bgra:&[u8],width:usize,height:usize,out:&mut[u8]){
    let plane=width*height;
    for y in 0..height{for x in 0..width{let i=(y*width+x)*4;let(b,g,r)=(bgra[i]as i32,bgra[i+1]as i32,bgra[i+2]as i32);out[y*width+x]=(((47*r+157*g+16*b+128)>>8)+16).clamp(16,235)as u8;}}
    for y in (0..height).step_by(2){for x in (0..width).step_by(2){let(mut r,mut g,mut b)=(0,0,0);for yy in y..y+2{for xx in x..x+2{let i=(yy*width+xx)*4;b+=bgra[i]as i32;g+=bgra[i+1]as i32;r+=bgra[i+2]as i32;}}r/=4;g/=4;b/=4;let i=plane+y/2*width+x;out[i]=(((-26*r-87*g+113*b+128)>>8)+128).clamp(16,240)as u8;out[i+1]=(((112*r-102*g-10*b+128)>>8)+128).clamp(16,240)as u8;}}
}
struct Clock {start:Instant,paused_at:Option<Instant>,paused:Duration}
impl Clock {fn new()->Self{Self{start:Instant::now(),paused_at:None,paused:Duration::ZERO}}fn update(&mut self,pause:bool)->Duration{let now=Instant::now();if pause&&self.paused_at.is_none(){self.paused_at=Some(now);}else if !pause{if let Some(t)=self.paused_at.take(){self.paused+=now-t;}}self.paused_at.unwrap_or(now)-self.start-self.paused}}

fn record(area:Area,fps:u32,audio:bool,path:&std::path::Path,stop:Arc<AtomicBool>,pause:Arc<AtomicBool>)->Result<(),String>{unsafe{
    CoInitializeEx(None,COINIT_MULTITHREADED).ok().map_err(|e|e.to_string())?;MFStartup(MF_VERSION,MFSTARTUP_FULL).map_err(|e|e.to_string())?;
    let result=(||->Result<(),String>{
        let mut attr=None;MFCreateAttributes(&mut attr,2).map_err(|e|e.to_string())?;let attr=attr.unwrap();attr.SetUINT32(&MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS,1).map_err(|e|e.to_string())?;
        let writer=MFCreateSinkWriterFromURL(PCWSTR(wide(&path.to_string_lossy()).as_ptr()),None,&attr).map_err(|e|format!("Không tạo được video MP4: {e}"))?;
        let vout=video_type(area,fps,true).map_err(|e|e.to_string())?;let video=writer.AddStream(&vout).map_err(|e|e.to_string())?;
        let vin=video_type(area,fps,false).map_err(|e|e.to_string())?;writer.SetInputMediaType(video,&vin,None).map_err(|e|format!("Bộ mã hóa video không nhận định dạng này: {e}"))?;
        let mut loopback=if audio{Some(Loopback::new().map_err(|e|format!("Không thu được âm thanh máy ({e}). Anh có thể tắt Thu tiếng máy trong Cài đặt rồi thử lại."))?)}else{None};
        let audio_stream=if audio{let out=audio_type(true).map_err(|e|e.to_string())?;let id=writer.AddStream(&out).map_err(|e|e.to_string())?;let input=audio_type(false).map_err(|e|e.to_string())?;writer.SetInputMediaType(id,&input,None).map_err(|e|e.to_string())?;Some(id)}else{None};
        writer.BeginWriting().map_err(|e|e.to_string())?;
        let mut grab=Grab::new(area)?;let mut pixels=vec![0u8;(area.w*area.h*3/2)as usize];let mut clock=Clock::new();let mut next=Duration::ZERO;let frame_duration=10_000_000/fps as i64;let mut audio_sent=0u64;let mut last_video=-1i64;
        let capture_result=(||->Result<(),String>{while !stop.load(Ordering::Relaxed){let paused=pause.load(Ordering::Relaxed);let elapsed=clock.update(paused);if let Some(a)=&mut loopback{a.poll(paused).map_err(|e|format!("Thiết bị âm thanh bị ngắt: {e}"))?;}if !paused&&elapsed>=next{
            let time=((elapsed.as_nanos()/100)as i64).max(last_video+1);last_video=time;
            nv12(grab.update(true)?,area.w as usize,area.h as usize,&mut pixels);
            writer.WriteSample(video,&sample(&pixels,time,frame_duration).map_err(|e|e.to_string())?).map_err(|e|format!("Không ghi được khung hình: {e}"))?;
            if let(Some(id),Some(a))=(audio_stream,loopback.as_mut()){let target=((time+frame_duration)as u64*48000/10_000_000).max(audio_sent);let count=(target-audio_sent).min(48000)as usize;if count>0{let data=a.take(count);writer.WriteSample(id,&sample(&data,(audio_sent*10_000_000/48000)as i64,(count as u64*10_000_000/48000)as i64).map_err(|e|e.to_string())?).map_err(|e|format!("Không ghi được âm thanh: {e}"))?;audio_sent+=count as u64;}}
            next=elapsed+Duration::from_secs_f64(1.0/fps as f64);
        }std::thread::sleep(Duration::from_millis(3));}Ok(())})();
        drop(loopback);let finalized=writer.Finalize().map_err(|e|format!("Không hoàn tất được video: {e}"));capture_result.and(finalized)
    })();let _=MFShutdown();CoUninitialize();result
}}

struct State {stop:Arc<AtomicBool>,pause:Arc<AtomicBool>,events:mpsc::Receiver<Result<(),String>>,clock:Clock,path:std::path::PathBuf,finished:bool,status:String}
thread_local!{static REC:RefCell<Option<State>>=const{RefCell::new(None)};}
pub fn run(mut area:Area,fps:u32,audio:bool)->Result<(),String>{
    // H.264 chroma requires even dimensions; retain the selected top-left.
    area.w-=area.w%2;area.h-=area.h%2;if area.w<4||area.h<4{return Err("Vùng quay quá nhỏ".into());}
    let path=super::output_path("Recording","mp4")?;let stop=Arc::new(AtomicBool::new(false));let pause=Arc::new(AtomicBool::new(false));let(tx,rx)=mpsc::channel();
    REC.with(|c|*c.borrow_mut()=Some(State{stop:stop.clone(),pause:pause.clone(),events:rx,clock:Clock::new(),path:path.clone(),finished:false,status:format!("{} × {} · {fps} FPS · {}",area.w,area.h,if audio{"Tiếng máy"}else{"Không tiếng"})}));
    unsafe{
        let cls=wide("tuanpc-deskRecordingPanel");let mut wc:WNDCLASSW=std::mem::zeroed();wc.lpfnWndProc=Some(proc);wc.lpszClassName=cls.as_ptr();wc.hCursor=LoadCursorW(std::ptr::null_mut(),IDC_ARROW);RegisterClassW(&wc);let m=screen::monitor_at(area.x,area.y);let hwnd=CreateWindowExW(WS_EX_TOPMOST|WS_EX_APPWINDOW,cls.as_ptr(),wide("TLauncher — Đang quay màn hình").as_ptr(),WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,m.x+24,m.y+24,590.min(m.w-40),185,std::ptr::null_mut(),std::ptr::null_mut(),std::ptr::null_mut(),std::ptr::null());
        if hwnd.is_null(){REC.with(|c|c.borrow_mut().take());return Err("Không mở được bảng quay".into());}
        SetWindowDisplayAffinity(hwnd,0x11);ShowWindow(hwnd,SW_SHOW);if std::env::var_os("TLAUNCHER_NATIVE_TRIAL_DIR").is_none(){RegisterHotKey(hwnd,81,MOD_CONTROL|MOD_SHIFT|MOD_NOREPEAT,b'R' as u32);RegisterHotKey(hwnd,82,MOD_CONTROL|MOD_SHIFT|MOD_NOREPEAT,b'P' as u32);}SetTimer(hwnd,1,200,None);
        std::thread::spawn(move||{let result=std::panic::catch_unwind(std::panic::AssertUnwindSafe(||record(area,fps,audio,&path,stop,pause))).unwrap_or_else(|_|Err("Bộ quay gặp lỗi; hãy thử lại với vùng nhỏ hơn".into()));let _=tx.send(result);});
        super::loop_messages();
    }REC.with(|c|c.borrow_mut().take());Ok(())
}
unsafe fn command(hwnd:HWND,id:usize){let close=REC.with(|c|{let mut g=c.borrow_mut();let s=g.as_mut().unwrap();match id{0=>{if !s.finished&&!s.stop.load(Ordering::Relaxed){s.pause.fetch_xor(true,Ordering::Relaxed);}},1=>{if s.finished{return true;}s.stop.store(true,Ordering::Relaxed);s.status="Đang hoàn tất file MP4…".into();},_=>{}}false});if close{DestroyWindow(hwnd);}InvalidateRect(hwnd,std::ptr::null(),0);}
unsafe extern "system" fn proc(hwnd:HWND,msg:u32,w:WPARAM,l:LPARAM)->LRESULT{match msg{
    WM_ERASEBKGND=>1,
    WM_TIMER=>{REC.with(|c|{let mut g=c.borrow_mut();let s=g.as_mut().unwrap();if let Ok(result)=s.events.try_recv(){s.finished=true;s.status=match result{Ok(())=>format!("Đã lưu: {}",s.path.display()),Err(e)=>format!("Lỗi: {e}")};}s.clock.update(s.finished || s.pause.load(Ordering::Relaxed));});InvalidateRect(hwnd,std::ptr::null(),0);0},
    WM_PAINT=>{let mut ps=std::mem::zeroed();let dc=BeginPaint(hwnd,&mut ps);let mut r=RECT{left:0,top:0,right:0,bottom:0};GetClientRect(hwnd,&mut r);let b=CreateSolidBrush(0x0025201B);FillRect(dc,&r,b);DeleteObject(b);let font=super::editor::make_font(15,500);let old=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT as i32);SetTextColor(dc,0x00FFFFFF);
        REC.with(|c|{let g=c.borrow();let s=g.as_ref().unwrap();let elapsed=s.clock.paused_at.unwrap_or_else(Instant::now)-s.clock.start-s.clock.paused;let seconds=elapsed.as_secs();let title=format!("{}  {:02}:{:02}    Phím quay: dừng · Phím tạm dừng: theo Cài đặt",if s.finished{"ĐÃ KẾT THÚC"}else if s.pause.load(Ordering::Relaxed){"TẠM DỪNG"}else{"● ĐANG QUAY"},seconds/60,seconds%60);let mut title_rect=RECT{left:14,top:12,right:r.right-14,bottom:42};DrawTextW(dc,wide(&title).as_ptr(),-1,&mut title_rect,DT_LEFT|DT_SINGLELINE|DT_END_ELLIPSIS);let mut status=RECT{left:14,top:44,right:r.right-14,bottom:86};DrawTextW(dc,wide(&s.status).as_ptr(),-1,&mut status,DT_LEFT|DT_WORDBREAK);
            for(i,label)in[if s.pause.load(Ordering::Relaxed){"Tiếp tục"}else{"Tạm dừng"},if s.finished{"Đóng"}else{"Dừng và lưu"},"Mở thư mục"].iter().enumerate(){let mut rr=RECT{left:14+i as i32*175,top:96,right:174+i as i32*175,bottom:128};let b=CreateSolidBrush(0x006A482C);FillRect(dc,&rr,b);DeleteObject(b);DrawTextW(dc,wide(label).as_ptr(),-1,&mut rr,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
        });SelectObject(dc,old);DeleteObject(font);EndPaint(hwnd,&ps);0},
    WM_LBUTTONUP=>{let x=(l as i16)as i32;let y=((l>>16)as i16)as i32;if(96..128).contains(&y){let i=(x-14)/175;if i==2{let path=REC.with(|c|c.borrow().as_ref().unwrap().path.parent().unwrap().to_path_buf());windows_sys::Win32::UI::Shell::ShellExecuteW(hwnd,wide("open").as_ptr(),wide(&path.to_string_lossy()).as_ptr(),std::ptr::null(),std::ptr::null(),SW_SHOWNORMAL);}else if x>=14{command(hwnd,i as usize);}}0},
    m if m==WM_APP+61=>{command(hwnd,0);0},
    WM_HOTKEY=>{command(hwnd,if w==82{0}else{1});0},WM_CLOSE=>{command(hwnd,1);0},
    WM_DESTROY=>{KillTimer(hwnd,1);UnregisterHotKey(hwnd,81);UnregisterHotKey(hwnd,82);REC.with(|c|if let Some(s)=c.borrow().as_ref(){s.stop.store(true,Ordering::Relaxed);});PostQuitMessage(0);0},_=>DefWindowProcW(hwnd,msg,w,l)
}}

#[cfg(test)]mod tests{use super::*;#[test]fn nv12_is_top_down_and_has_bounded_color_planes(){let bgra=[0,0,0,255,0,0,0,255,255,255,255,255,255,255,255,255];let mut out=[0;6];nv12(&bgra,2,2,&mut out);assert_eq!(&out[..2],&[16,16]);assert_eq!(&out[2..4],&[235,235]);assert_eq!(&out[4..],&[128,128]);}#[test]fn encoder_writes_a_decodable_mp4(){unsafe{let _=CoInitializeEx(None,COINIT_MULTITHREADED);MFStartup(MF_VERSION,MFSTARTUP_FULL).unwrap();let path=std::env::temp_dir().join(format!("TLauncher-encoder-test-{}.mp4",std::process::id()));let writer=MFCreateSinkWriterFromURL(PCWSTR(wide(&path.to_string_lossy()).as_ptr()),None,None).unwrap();let area=Area{x:0,y:0,w:64,h:64};let id=writer.AddStream(&video_type(area,30,true).unwrap()).unwrap();writer.SetInputMediaType(id,&video_type(area,30,false).unwrap(),None).unwrap();writer.BeginWriting().unwrap();let mut data=vec![128;64*64*3/2];data[..64*64].fill(80);for i in 0..6{writer.WriteSample(id,&sample(&data,i*333333,333333).unwrap()).unwrap();}writer.Finalize().unwrap();drop(writer);let reader=MFCreateSourceReaderFromURL(PCWSTR(wide(&path.to_string_lossy()).as_ptr()),None).unwrap();let t=reader.GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM.0 as u32,0).unwrap();assert_eq!(t.GetGUID(&MF_MT_SUBTYPE).unwrap(),MFVideoFormat_H264);drop(reader);std::fs::remove_file(path).unwrap();let _=MFShutdown();}}}
