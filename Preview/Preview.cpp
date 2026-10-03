#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include "../Source/Clanka/Simulation.h"

using splash::Vec;
using splash::Action;
static splash::Simulation sim;
static HWND windowHandle;
static HDC bufferDC;
static HBITMAP bitmap;
static HGDIOBJ originalBitmap;
static int width=1440,height=900,cameraIndex=0;
static double scale=1,accumulator=0;
static bool smoke=false;
static double wallTime=0;
static std::wstring notice;
static double noticeUntil=0;
static COLORREF ink=RGB(8,20,30),panel=RGB(15,34,46),mint=RGB(100,242,187),muted=RGB(132,164,179),white=RGB(234,245,248),amber=RGB(255,182,75);
struct Point { double x,y,depth; };
struct Button { RECT r; int id; };
static std::vector<Button> buttons;
static Vec eye{-1800,-2300,1700},forward,right,up;
static Vec cross(Vec a,Vec b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
static void camera() {
    Vec positions[]={{-1800,-2300,1700},{-1800,0,950},{600,-1600,2800}};
    eye=positions[cameraIndex]; forward=(Vec{950,0,440}-eye).unit(); right=cross(forward,{0,0,1}).unit(); up=cross(right,forward);
}
static Point project(Vec v) { Vec d=v-eye; double depth=std::max(50.0,d.dot(forward)); return {785+d.dot(right)*1080/depth,460-d.dot(up)*1080/depth,depth}; }
static int sx(double v) {return int(v*scale);}
static void rect(double x,double y,double w,double h,COLORREF c) { RECT r{sx(x),sx(y),sx(x+w),sx(y+h)}; HBRUSH b=CreateSolidBrush(c); FillRect(bufferDC,&r,b); DeleteObject(b); }
static void text(const std::wstring& s,double x,double y,int size,COLORREF c,bool bold=false) {
    HFONT font=CreateFontW(-sx(size),0,0,0,bold?FW_BOLD:FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    auto old=SelectObject(bufferDC,font); SetTextColor(bufferDC,c); SetBkMode(bufferDC,TRANSPARENT); TextOutW(bufferDC,sx(x),sx(y),s.c_str(),int(s.size())); SelectObject(bufferDC,old); DeleteObject(font);
}
static std::wstring number(double n,int decimals=0) {std::wostringstream s;s<<std::fixed<<std::setprecision(decimals)<<n;return s.str();}
static std::wstring wide(const std::string& s) {return std::wstring(s.begin(),s.end());}
static void line(double x,double y,double x2,double y2,COLORREF c,int thickness=1) {HPEN p=CreatePen(PS_SOLID,std::max(1,sx(thickness)),c);auto old=SelectObject(bufferDC,p);MoveToEx(bufferDC,sx(x),sx(y),nullptr);LineTo(bufferDC,sx(x2),sx(y2));SelectObject(bufferDC,old);DeleteObject(p);}
static void line3(Vec a,Vec b,COLORREF c,int w=1) {auto p=project(a),q=project(b);line(p.x,p.y,q.x,q.y,c,w);}
static void ellipse(double x,double y,double rx,double ry,COLORREF color) {auto brush=CreateSolidBrush(color);auto pen=CreatePen(PS_SOLID,1,color);auto ob=SelectObject(bufferDC,brush),op=SelectObject(bufferDC,pen);Ellipse(bufferDC,sx(x-rx),sx(y-ry),sx(x+rx),sx(y+ry));SelectObject(bufferDC,ob);SelectObject(bufferDC,op);DeleteObject(brush);DeleteObject(pen);}
static void poly(const std::vector<Vec>& verts,COLORREF color) {std::vector<POINT> ps;for(auto v:verts){auto p=project(v);ps.push_back({sx(p.x),sx(p.y)});}auto b=CreateSolidBrush(color);auto p=CreatePen(PS_SOLID,1,color);auto ob=SelectObject(bufferDC,b),op=SelectObject(bufferDC,p);Polygon(bufferDC,ps.data(),int(ps.size()));SelectObject(bufferDC,ob);SelectObject(bufferDC,op);DeleteObject(b);DeleteObject(p);}
static void button(int id,const std::wstring& label,int x,int y,int w,COLORREF color) {
    POINT mouse;GetCursorPos(&mouse);ScreenToClient(windowHandle,&mouse);RECT r{sx(x),sx(y),sx(x+w),sx(y+38)};
    rect(x,y,w,38,PtInRect(&r,mouse)?RGB(41,74,83):RGB(26,48,61));rect(x,y,3,38,color);text(label,x+12,y+9,15,color,true);buttons.push_back({r,id});
}
static std::wstring folder() {wchar_t p[MAX_PATH];GetModuleFileNameW(nullptr,p,MAX_PATH);std::wstring s=p;return s.substr(0,s.find_last_of(L"\\/"));}
static bool saveLog() {
    auto p=folder()+L"\\SessionLog.csv";std::ofstream out(p);out<<"seconds,event\n";for(const auto& e:sim.events)out<<e.time<<",\""<<e.message<<"\"\n";out<<sim.time<<",\"SUMMARY popped="<<sim.popped<<" escaped="<<sim.escaped<<" tank="<<sim.tank<<"\"\n";
    notice=out?L"Saved SessionLog.csv beside the executable":L"Unable to write session log";noticeUntil=wallTime+4;return bool(out);
}
static void screenshot(const std::wstring& path) {
    BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=width;bi.bmiHeader.biHeight=-height;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
    std::vector<unsigned char> pixels(size_t(width)*height*4);SelectObject(bufferDC,originalBitmap);int ok=GetDIBits(bufferDC,bitmap,0,height,pixels.data(),&bi,DIB_RGB_COLORS);SelectObject(bufferDC,bitmap);if(!ok)return;
    BITMAPFILEHEADER bh{};bh.bfType=0x4d42;bh.bfOffBits=sizeof(bh)+sizeof(BITMAPINFOHEADER);bh.bfSize=bh.bfOffBits+DWORD(pixels.size());std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<char*>(&bh),sizeof(bh));f.write(reinterpret_cast<char*>(&bi.bmiHeader),sizeof(bi.bmiHeader));f.write(reinterpret_cast<char*>(pixels.data()),pixels.size());
}
static void action(int id) {
    if(id==20){cameraIndex=(cameraIndex+1)%3;camera();return;}
    if(id==21){saveLog();return;}
    if(id==22){DestroyWindow(windowHandle);return;}
    if(id>=100){int i=id-100;if(i<int(sim.balloons.size())&&sim.balloons[i].alive&&!sim.balloons[i].protectedZone){sim.selected=i;sim.automatic=false;sim.verified=false;notice=L"Track selected. Press V to verify water.";noticeUntil=wallTime+4;}return;}
    Action actions[]={Action::Run,Action::Verify,Action::ToggleAuto,Action::Next,Action::Pause,Action::Refill,Action::Stop,Action::Reset,Action::Calm,Action::Breeze,Action::Gusts};
    if(id>=0&&id<11){if(id==6&&sim.stopped)sim.action(Action::ReleaseStop);else sim.action(actions[id]);if(id==7)sim.continuous=false;}
}
static void draw() {
    buttons.clear();rect(0,0,1440,900,RGB(18,41,55));
    // A software-projected 3D view shares exactly the same C++ game state as Unreal.
    int saved=SaveDC(bufferDC);IntersectClipRect(bufferDC,sx(288),sx(95),sx(1170),sx(811));
    for(int y=95;y<815;y+=4){int t=(y-95)*30/720;rect(288,y,882,4,RGB(23+t/3,48+t/2,66+t/2));}
    poly({{-2000,-2500,0},{3400,-2500,0},{3400,2500,0},{-2000,2500,0}},RGB(30,55,63));
    poly({{0,620,2},{2800,620,2},{2800,1200,2},{0,1200,2}},RGB(73,61,44));
    for(int i=-16;i<=22;++i)line3({double(i*150),-2300,3},{double(i*150),2300,3},RGB(47,76,83));
    for(int i=-15;i<=15;++i)line3({-2200,double(i*150),3},{3300,double(i*150),3},RGB(47,76,83));
    line3({0,620,5},{2800,620,5},amber,3);
    for(int i=0;i<8;++i){Vec p{2700.,-1250.+i*350,0};line3(p,p+Vec{0,0,180},RGB(69,112,123),5);auto q=project(p+Vec{0,0,180});ellipse(q.x,q.y,3,3,mint);}
    auto base=project({0,0,25}),head=project(sim.nozzle),nozzle=project(sim.nozzle+sim.aim*200);
    ellipse(base.x,base.y,44,17,RGB(19,35,41));line(base.x,base.y,head.x,head.y,RGB(98,136,145),26);ellipse(head.x,head.y,24,21,RGB(179,209,207));line(head.x,head.y,nozzle.x,nozzle.y,RGB(30,116,137),13);ellipse(nozzle.x,nozzle.y,5,5,mint);
    for(const auto& d:sim.drops){auto p=project(d.p);ellipse(p.x,p.y,2.8,1.7,RGB(115,224,255));}
    std::vector<int> order;for(int i=0;i<int(sim.balloons.size());++i)order.push_back(i);std::sort(order.begin(),order.end(),[](int a,int b){return project(sim.balloons[a].p).depth>project(sim.balloons[b].p).depth;});
    const COLORREF colors[]={RGB(245,96,117),RGB(70,183,247),RGB(255,189,66),RGB(175,128,251),RGB(86,220,180)};
    for(int i:order){const auto& b=sim.balloons[i];if(!b.alive)continue;auto p=project(b.p),ground=project({b.p.x,b.p.y,4});double r=52000/p.depth*(1+b.wet*.16);
        ellipse(ground.x,ground.y,r*.8,r*.2,RGB(23,44,48));line3(b.p-Vec{0,0,64},b.p-Vec{0,0,155},RGB(160,189,193));
        COLORREF c=b.protectedZone?RGB(140,145,145):colors[i%5];ellipse(p.x,p.y,r,r*1.26,c);ellipse(p.x-r*.29,p.y-r*.4,r*.2,r*.28,b.protectedZone?RGB(180,182,181):RGB(245,251,249));
        if(i==sim.selected){double x=p.x-r-8,y=p.y-r*1.26-8,w=r*2+16,h=r*2.52+16;line(x,y,x+12,y,mint,2);line(x,y,x,y+12,mint,2);line(x+w,y,x+w-12,y,mint,2);line(x+w,y,x+w,y+12,mint,2);line(x,y+h,x+12,y+h,mint,2);line(x+w,y+h,x+w-12,y+h,mint,2);rect(p.x-25,p.y+r*1.26+13,50,4,ink);rect(p.x-25,p.y+r*1.26+13,50*b.wet,4,mint);}
        text(L"B-"+number(b.id)+(b.protectedZone?L"  PROTECTED":L""),p.x-r,p.y-r*1.26-28,12,b.protectedZone?amber:white,true);
        RECT hit{sx(p.x-r),sx(p.y-r*1.26),sx(p.x+r),sx(p.y+r*1.26)};buttons.push_back({hit,100+i});
    }
    RestoreDC(bufferDC,saved);
    rect(0,0,1440,95,ink);rect(26,24,5,47,mint);text(L"CLANKA",47,15,34,white,true);text(L"S P L A S H   L A B",48,57,12,muted);
    text(L"VIRTUAL BALLOON RANGE",500,24,19,white,true);text(L"LEGACY TIMED C++ PREVIEW  /  UNREAL IS THE MAIN DEMO",500,55,11,muted);
    text(number(sim.score),1120,17,34,mint,true);text(L"SCORE",1124,57,11,muted);int remain=int(180-sim.time);text(number(remain/60)+L":"+(remain%60<10?L"0":L"")+number(remain%60),1300,21,28,white,true);text(L"REMAINING",1300,56,11,muted);
    rect(18,115,270,650,panel);text(L"OPERATOR CONSOLE",36,132,13,muted,true);text(wide(sim.status()),36,166,18,sim.stopped?RGB(255,108,100):mint,true);
    button(0,L"RUN DEMO  /  ENTER",36,209,234,mint);button(1,sim.verified?L"DISARM WATER  /  V":L"VERIFY WATER  /  V",36,255,234,mint);button(2,sim.automatic?L"AUTO SELECT  /  A":L"MANUAL SELECT  /  A",36,301,234,muted);button(3,L"NEXT BALLOON  /  TAB",36,347,234,muted);button(4,sim.paused?L"RESUME  /  SPACE":L"PAUSE  /  SPACE",36,393,234,muted);button(5,L"REFILL TANK  /  F",36,439,234,RGB(105,209,252));button(6,sim.stopped?L"RELEASE STOP  /  T":L"EMERGENCY STOP  /  E",36,485,234,RGB(255,124,110));
    text(L"WATER RESERVE",36,548,12,muted);text(number(sim.tank)+L"%",226,544,17,white,true);rect(36,578,234,8,ink);rect(36,578,234*sim.tank/100,8,RGB(95,206,251));text(L"WIND SCENARIO",36,612,12,muted);button(8,L"1",36,640,66,sim.scenario==0?mint:muted);button(9,L"2",120,640,66,sim.scenario==1?mint:muted);button(10,L"3",204,640,66,sim.scenario==2?mint:muted);const wchar_t* names[]={L"CALM",L"BREEZE",L"GUSTS"};text(names[sim.scenario],36,700,18,white,true);
    rect(1170,115,252,307,panel);text(L"RANGE OVERVIEW",1190,133,13,muted,true);rect(1195,177,203,200,ink);rect(1349,177,49,200,RGB(72,52,30));for(int i=1;i<4;++i){rect(1195+i*51,177,1,200,RGB(29,56,66));rect(1195,177+i*50,203,1,RGB(29,56,66));}for(int i=0;i<int(sim.balloons.size());++i){auto& b=sim.balloons[i];if(!b.alive)continue;rect(1195+(b.p.y+1200)/2400*203,374-b.p.x/2500*190,5,5,b.protectedZone?amber:(i==sim.selected?mint:muted));}text(L"ORANGE = NO-SPRAY ZONE",1189,395,12,amber);
    rect(1170,439,252,213,panel);text(L"SESSION TELEMETRY",1190,457,13,muted,true);text(L"Splashed",1190,498,16,white);text(number(sim.popped),1350,498,17,mint,true);text(L"Exited range",1190,533,16,white);text(number(sim.escaped),1350,533,17,white);text(L"Water time",1190,568,16,white);text(number(sim.sprayingTime,1)+L"s",1333,568,16,white);text(sim.automatic?L"AUTOMATIC SELECTION":L"MANUAL SELECTION",1190,612,12,mint);
    button(20,L"CHANGE CAMERA  /  C",1170,675,252,muted);button(7,L"NEW SESSION  /  R",1170,725,252,muted);button(21,L"SAVE SESSION LOG  /  L",1170,775,252,muted);
    rect(0,830,1440,70,ink);text(L"LIVE EVENTS",25,842,11,mint,true);if(!sim.events.empty())text(wide(sim.events.back().message),25,863,15,white);text(L"CLICK BALLOON + V TO VERIFY   /   ESC TO QUIT",1000,867,11,muted);
    if(wallTime<noticeUntil){rect(415,743,675,42,panel);text(notice,435,754,17,mint,true);}
    if(!sim.running){rect(392,255,710,355,ink);rect(392,255,710,4,mint);text(L"MAKE A SPLASH.",432,282,42,white,true);text(L"A small water turret. A sky full of balloons.",432,343,19,muted);text(L"1   Run the demo to track and spray automatically.",432,390,18,white);text(L"2   Try wind presets, cameras, and manual selection.",432,427,18,white);text(L"3   Refill the tank. Score as much as you can in 3 minutes.",432,464,18,white);text(L"Virtual targets and arcade water. No hardware connection.",432,514,14,muted);button(0,L"START DEMO",432,552,250,mint);}
    if(sim.complete){rect(425,320,630,180,ink);text(L"SESSION COMPLETE",465,346,30,mint,true);text(number(sim.popped)+L" balloons splashed  /  "+number(sim.score)+L" points",465,400,22,white);text(L"R: new session    L: save your session log",465,449,18,muted);}
}
static LRESULT CALLBACK proc(HWND h,UINT msg,WPARAM w,LPARAM l) {
    switch(msg){
    case WM_CREATE:SetTimer(h,1,16,nullptr);return 0;
    case WM_SIZE:{width=LOWORD(l);height=HIWORD(l);if(width==0||height==0)return 0;scale=std::min(width/1440.0,height/900.0);HDC dc=GetDC(h);if(!bufferDC)bufferDC=CreateCompatibleDC(dc);if(bitmap){SelectObject(bufferDC,originalBitmap);DeleteObject(bitmap);}bitmap=CreateCompatibleBitmap(dc,width,height);originalBitmap=SelectObject(bufferDC,bitmap);ReleaseDC(h,dc);return 0;}
    case WM_ERASEBKGND:return 1;
    case WM_TIMER:{static LARGE_INTEGER previous{},frequency{};LARGE_INTEGER now;QueryPerformanceCounter(&now);if(!frequency.QuadPart)QueryPerformanceFrequency(&frequency);double dt=previous.QuadPart?double(now.QuadPart-previous.QuadPart)/frequency.QuadPart:.016;previous=now;wallTime+=dt;accumulator+=std::min(.1,dt);int old=sim.popped;while(accumulator>=splash::Simulation::Step){sim.tick();accumulator-=splash::Simulation::Step;}if(sim.popped>old){notice=L"SPLASH!";noticeUntil=wallTime+1.3;}InvalidateRect(h,nullptr,FALSE);if(smoke&&wallTime>13){draw();screenshot(folder()+L"\\Demo.bmp");std::ofstream report(folder()+L"\\DemoResult.json");report<<"{\"popped\":"<<sim.popped<<",\"seconds\":"<<sim.time<<",\"tank\":"<<sim.tank<<",\"rendered\":true}";report.close();DestroyWindow(h);}return 0;}
    case WM_PAINT:{PAINTSTRUCT p;HDC dc=BeginPaint(h,&p);if(bitmap){RECT r{0,0,width,height};FillRect(bufferDC,&r,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));draw();BitBlt(dc,0,0,width,height,bufferDC,0,0,SRCCOPY);}EndPaint(h,&p);return 0;}
    case WM_LBUTTONDOWN:{POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};for(auto i=buttons.rbegin();i!=buttons.rend();++i)if(PtInRect(&i->r,p)){action(i->id);break;}return 0;}
    case WM_KEYDOWN:if(l&(1LL<<30))return 0;switch(w){case VK_RETURN:action(0);break;case 'V':action(1);break;case 'A':action(2);break;case VK_TAB:action(3);break;case VK_SPACE:action(4);break;case 'F':action(5);break;case 'E':sim.action(Action::Stop);break;case 'T':sim.action(Action::ReleaseStop);break;case 'R':action(7);break;case '1':action(8);break;case '2':action(9);break;case '3':action(10);break;case 'C':action(20);break;case 'L':action(21);break;case VK_ESCAPE:DestroyWindow(h);break;}return 0;
    case WM_DESTROY:saveLog();KillTimer(h,1);PostQuitMessage(0);return 0;
    }return DefWindowProcW(h,msg,w,l);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR cmd,int show) {
    SetProcessDPIAware();sim.continuous=false;smoke=wcsstr(cmd,L"--demo-test")!=nullptr;if(smoke)sim.action(Action::Run);camera();
    WNDCLASSW wc{};wc.lpfnWndProc=proc;wc.hInstance=instance;wc.lpszClassName=L"ClankaSplashPreview";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);
    RECT r{0,0,1440,900};AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE);windowHandle=CreateWindowW(wc.lpszClassName,L"Clanka Splash Lab | Legacy Timed C++ Preview",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,nullptr,nullptr,instance,nullptr);if(!windowHandle)return 1;
    ShowWindow(windowHandle,show);MSG m{};while(GetMessageW(&m,nullptr,0,0)>0){TranslateMessage(&m);DispatchMessageW(&m);}if(bitmap){SelectObject(bufferDC,originalBitmap);DeleteObject(bitmap);}if(bufferDC)DeleteDC(bufferDC);return 0;
}
