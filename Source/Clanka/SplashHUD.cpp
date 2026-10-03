#include "SplashGame.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

static const FLinearColor HUDInk(.012f,.026f,.048f,.97f), HUDPanel(.022f,.048f,.077f,.92f), HUDMint(.32f,1,.76f,1), HUDMuted(.57f,.72f,.8f,1), HUDWhite(.94f,.98f,1,1), HUDGold(1,.72f,.3f,1), HUDRed(1,.35f,.32f,1);
ASplashRange* ASplashHUD::Range() const { for(TActorIterator<ASplashRange> I(GetWorld());I;++I) return *I; return nullptr; }
void ASplashHUD::Text(const FString& S,float X,float Y,FLinearColor C,float Size) { DrawText(S,C,OffsetX+X*Scale,OffsetY+Y*Scale,GEngine->GetMediumFont(),FMath::Max(.8f,Size*1.25f)*Scale,false); }
void ASplashHUD::Box(float X,float Y,float W,float H,FLinearColor C) { DrawRect(C,OffsetX+X*Scale,OffsetY+Y*Scale,W*Scale,H*Scale); }
void ASplashHUD::Line(float X,float Y,float X2,float Y2,FLinearColor C,float T) { DrawLine(OffsetX+X*Scale,OffsetY+Y*Scale,OffsetX+X2*Scale,OffsetY+Y2*Scale,C,T*Scale); }
void ASplashHUD::Button(FName ID,const FString& Label,float X,float Y,float W,FLinearColor C) {
    float MX=0,MY=0; GetOwningPlayerController()->GetMousePosition(MX,MY);
    const bool Hover=MX>OffsetX+X*Scale && MX<OffsetX+(X+W)*Scale && MY>OffsetY+Y*Scale && MY<OffsetY+(Y+48)*Scale;
    Box(X,Y,W,48,Hover?FLinearColor(.09f,.22f,.27f,.98f):FLinearColor(.035f,.085f,.12f,.97f)); Box(X,Y,3,48,C);
    Text(Label,X+18,Y+14,C,.89f); AddHitBox(FVector2D(OffsetX+X*Scale,OffsetY+Y*Scale),FVector2D(W*Scale,48*Scale),ID,true);
}
void ASplashHUD::DrawHUD() {
    Super::DrawHUD(); auto* R=Range(); if(!Canvas || !R) return; auto& S=R->Sim;
    Scale=FMath::Min(Canvas->SizeX/1600.f,Canvas->SizeY/900.f); OffsetX=(Canvas->SizeX-1600*Scale)/2; OffsetY=(Canvas->SizeY-900*Scale)/2;
    // A compact status bar keeps the 3D arena visible.
    Box(24,20,1552,84,HUDInk); Box(44,40,4,40,HUDMint);
    Text(TEXT("CLANKA"),62,31,HUDWhite,1.5f); Text(TEXT("S P L A S H   L A B"),64,69,HUDMuted,.55f);
    Text(TEXT("AUTONOMOUS BALLOON RANGE"),390,39,HUDWhite,.99f); Text(TEXT("TRACK  /  ALIGN  /  SPRAY  /  REPEAT"),392,69,HUDMuted,.55f);
    Text(FString::Printf(TEXT("%03d"),S.popped),1050,31,HUDMint,1.55f); Text(TEXT("BALLOONS POPPED"),1053,70,HUDMuted,.53f);
    Text(FString::Printf(TEXT("%02d:%02d"),int(S.time)/60,int(S.time)%60),1350,36,HUDWhite,1.25f); Text(TEXT("CONTINUOUS SESSION"),1349,70,HUDMuted,.53f);
    if(!S.running) {
        // Designed to be left on screen: one unmistakable ON button.
        Box(76,224,600,438,HUDInk); Box(76,224,600,4,HUDMint);
        Text(TEXT("WATER. BALLOONS."),118,269,HUDWhite,2.5f);
        Text(TEXT("FULLY AUTOMATIC."),118,316,HUDMint,2.5f);
        Text(TEXT("A little water cannon with a big job."),120,383,HUDMuted,1.03f);
        Text(TEXT("01   Switch ON to start tracking."),120,443,HUDWhite,.98f);
        Text(TEXT("02   Press ENGAGE. Enjoy the show."),120,479,HUDWhite,.98f);
        Button(S.stopped?TEXT("release"):TEXT("on"),S.stopped?TEXT("RELEASE STOP TO RESTART"):TEXT("POWER ON    /    CLICK TO START"),120,546,510,S.stopped?HUDRed:HUDMint);
        Text(TEXT("Automatic refills. Endless balloons. No time limit."),120,616,HUDMuted,.7f);
        Box(984,642,535,128,HUDPanel); Text(TEXT("SIMULATION SANDBOX"),1008,663,HUDMint,.73f);
        Text(TEXT("Everything happens here in the virtual range."),1008,696,HUDWhite,.83f);
        Text(TEXT("Real-time C++ simulation  +  Unreal Engine"),1008,727,HUDMuted,.67f);
        Button(TEXT("sound"),R->bSound?TEXT("SOUND ON  /  M"):TEXT("SOUND OFF  /  M"),76,752,260,HUDMuted);
        Button(TEXT("quit"),TEXT("QUIT"),351,752,155,HUDMuted);
        Text(TEXT("C  CAMERA     SCROLL  ZOOM"),1110,840,HUDMuted,.7f);
        return;
    }
    Box(24,124,306,98,HUDPanel); Text(TEXT("SYSTEM STATUS"),45,140,HUDMuted,.58f);
    Text(UTF8_TO_TCHAR(S.status()),45,174,S.stopped?HUDRed:(S.verified?HUDMint:HUDGold),.9f);
    if(!S.verified && !S.stopped && !S.paused) {
        Box(470,126,660,76,HUDInk); Text(TEXT("TRACKING ONLINE. READY TO ENGAGE."),498,143,HUDMint,.92f); Text(TEXT("Press ENGAGE below to authorize automatic water."),498,174,HUDMuted,.73f);
    }
    // Every annotation is projected from live backend positions.
    TArray<FVector2D> UsedLabels;
    for(int I=0;I<static_cast<int>(S.balloons.size());++I) {
        const auto& B=S.balloons[I]; if(!B.alive) continue; FVector2D P;
        if(!GetOwningPlayerController()->ProjectWorldLocationToScreen(FVector(B.p.x,B.p.y,B.p.z),P)) continue;
        float X=(P.X-OffsetX)/Scale,Y=(P.Y-OffsetY)/Scale; if(X<350 || X>1280 || Y<215 || Y>737) continue;
        const bool Selected=I==S.selected; FLinearColor C=B.protectedZone?HUDGold:(Selected?HUDMint:HUDWhite);
        if(Selected) {
            constexpr float D=42,L=13;
            Line(X-D,Y-D,X-D+L,Y-D,C,2); Line(X-D,Y-D,X-D,Y-D+L,C,2);
            Line(X+D,Y-D,X+D-L,Y-D,C,2); Line(X+D,Y-D,X+D,Y-D+L,C,2);
            Line(X-D,Y+D,X-D+L,Y+D,C,2); Line(X+D,Y+D,X+D-L,Y+D,C,2);
            Box(X-34,Y+50,68,4,HUDInk); Box(X-34,Y+50,68*B.wet,4,HUDMint);
        }
        // Only label selected/protected tracks; other balloons keep the view clear.
        if(Selected || B.protectedZone) {
            float LY=Y-67; for(const auto& Existing:UsedLabels) if(FMath::Abs(Existing.X-X)<125 && FMath::Abs(Existing.Y-LY)<22) LY-=24;
            UsedLabels.Add(FVector2D(X,LY));
            Text(FString::Printf(TEXT("B-%02d  %s"),B.id,B.protectedZone?TEXT("ZONE"):TEXT("LOCK")),X-40,LY,C,.63f);
        }
    }
    for(const auto& F:R->FloatingScores) { FVector2D P; if(GetOwningPlayerController()->ProjectWorldLocationToScreen(F.Position,P)) { FLinearColor C=HUDMint; C.A=FMath::Clamp(1-F.Age/1.15f,0.f,1.f); Text(TEXT("SPLASH!"),(P.X-OffsetX)/Scale-38,(P.Y-OffsetY)/Scale-20,C,1.0f); } }
    // Compact radar and water readout.
    Box(1310,125,266,290,HUDPanel); Text(TEXT("LIVE TRACKS"),1332,145,HUDMuted,.63f);
    Box(1333,184,217,166,FLinearColor(.009f,.025f,.043f,.97f)); Box(1497,184,53,166,FLinearColor(.23f,.14f,.065f,.75f));
    for(int I=1;I<4;++I) { Box(1333+I*54,184,1,166,FLinearColor(.05f,.11f,.16f)); Box(1333,184+I*41,217,1,FLinearColor(.05f,.11f,.16f)); }
    for(int I=0;I<static_cast<int>(S.balloons.size());++I) { const auto& B=S.balloons[I]; if(!B.alive) continue; float X=1333+(B.p.y+1200)/2400*217,Y=345-B.p.x/2500*155; Box(X-3,Y-3,I==S.selected?7:4,I==S.selected?7:4,B.protectedZone?HUDGold:(I==S.selected?HUDMint:HUDMuted)); }
    Text(TEXT("AMBER  /  PROTECTED ZONE"),1332,374,HUDGold,.56f);
    Box(1310,435,266,153,HUDPanel); Text(TEXT("WATER RESERVE"),1332,455,HUDMuted,.63f);
    Text(FString::Printf(TEXT("%.0f%%"),S.tank),1332,483,FLinearColor(.33f,.8f,1),1.3f);
    Box(1332,524,222,7,HUDInk); Box(1332,524,222*S.tank/100,7,FLinearColor(.2f,.74f,1));
    Text(S.refill>0?FString::Printf(TEXT("REFILLING  %.1fs"),S.refill):TEXT("AUTO REFILL ENABLED"),1332,552,HUDMint,.57f);
    const auto* T=S.target();
    Box(24,240,306,145,HUDPanel); Text(TEXT("SELECTED TRACK"),45,256,HUDMuted,.6f);
    Text(T?FString::Printf(TEXT("BALLOON  B-%02d"),T->id):TEXT("SCANNING..."),45,287,HUDWhite,.9f);
    Text(T?FString::Printf(TEXT("ACQUISITION  %.0f%%"),T->confidence*100):TEXT("WAITING FOR ELIGIBLE BALLOON"),45,325,HUDMint,.62f);
    Text(TEXT("Click any balloon to prioritize it."),45,355,HUDMuted,.58f);
    Box(24,404,306,117,HUDPanel); Text(TEXT("WIND PROFILE"),45,420,HUDMuted,.6f);
    const TCHAR* Wind[]={TEXT("CALM"),TEXT("BREEZE"),TEXT("GUSTS")}; Text(Wind[S.scenario],45,451,HUDWhite,.93f);
    Text(TEXT("1  CALM     2  BREEZE     3  GUSTS"),45,487,HUDMuted,.55f);
    Box(24,778,1552,98,HUDInk);
    Button(TEXT("engage"),S.verified?TEXT("DISENGAGE  /  V"):TEXT("ENGAGE  /  ENTER"),45,797,262,S.verified?HUDGold:HUDMint);
    Button(TEXT("camera"),TEXT("CAMERA  /  C"),321,797,200,HUDMuted);
    Button(TEXT("pause"),S.paused?TEXT("RESUME"):TEXT("PAUSE  /  SPACE"),535,797,211,HUDMuted);
    Button(TEXT("stop"),S.stopped?TEXT("RELEASE STOP"):TEXT("STOP  /  E"),760,797,215,HUDRed);
    Button(TEXT("sound"),R->bSound?TEXT("SOUND ON"):TEXT("SOUND OFF"),989,797,174,HUDMuted);
    Button(TEXT("off"),TEXT("POWER OFF"),1177,797,201,HUDMuted);
    Button(TEXT("help"),TEXT("?"),1392,797,66,HUDMuted);
    Button(TEXT("reset"),TEXT("RESET"),1472,797,84,HUDMuted);
    if(!S.events.empty()) Text(UTF8_TO_TCHAR(S.events.back().message.c_str()),47,854,HUDMuted,.55f);
    if(R->NoticeSeconds>0) { Box(444,707,713,44,HUDPanel); Text(R->Notice,460,721,HUDMint,.7f); }
    if(S.stopped || S.paused) {
        Box(516,328,568,158,HUDInk); Text(S.stopped?TEXT("SYSTEM STOPPED"):TEXT("SIMULATION PAUSED"),552,355,S.stopped?HUDRed:HUDWhite,1.4f);
        Text(S.stopped?TEXT("Release the stop, then ENGAGE to restart water."):TEXT("Press Space or RESUME to continue."),552,412,HUDMuted,.8f);
    }
    if(R->bHelp) {
        Box(407,235,786,363,HUDInk); Text(TEXT("AUTONOMOUS BY DESIGN"),447,268,HUDMint,1.4f);
        Text(TEXT("ON starts tracking. ENGAGE starts the water."),447,328,HUDWhite,.96f);
        Text(TEXT("The cannon chooses a balloon, aligns, and sprays it."),447,366,HUDWhite,.92f);
        Text(TEXT("Amber zone balloons are excluded. Refills are automatic."),447,404,HUDWhite,.9f);
        Text(TEXT("C  camera     Scroll  zoom     1/2/3  wind     M  sound"),447,461,HUDMuted,.82f);
        Text(TEXT("L  save log     R  reset     Esc  power off / quit"),447,498,HUDMuted,.82f);
        Button(TEXT("close_help"),TEXT("GOT IT"),447,543,220,HUDMint);
    }
}
void ASplashHUD::NotifyHitBoxClick(FName Name) {
    auto* R=Range(); if(!R) return; using A=splash::Action;
    if(Name==TEXT("on")) R->StartGame(true);
    else if(Name==TEXT("release")) R->Command(A::ReleaseStop);
    else if(Name==TEXT("engage")) R->Command(R->Sim.verified?A::Verify:A::Engage);
    else if(Name==TEXT("pause")) R->Command(A::Pause);
    else if(Name==TEXT("stop")) R->Command(R->Sim.stopped?A::ReleaseStop:A::Stop);
    else if(Name==TEXT("off")) R->ReturnToMenu();
    else if(Name==TEXT("camera")) R->ChangeCamera();
    else if(Name==TEXT("sound")) R->ToggleSound();
    else if(Name==TEXT("reset")) R->Command(A::Reset);
    else if(Name==TEXT("help")) R->bHelp=!R->bHelp;
    else if(Name==TEXT("close_help")) R->bHelp=false;
    else if(Name==TEXT("quit")) FGenericPlatformMisc::RequestExit(false);
}
